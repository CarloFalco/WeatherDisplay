/**
 * @file OtaManager.cpp
 * @brief Implementazione dell'OTA nodi da GitHub.
 */
#include "OtaManager.h"

#include <HTTPClient.h>
#include <LittleFS.h>
#include <Logger.h>
#include <WiFiClientSecure.h>

#include "../../include/version.h"

namespace {
constexpr const char* kTag = "OTA";
constexpr uint32_t kTaskStack = 12288;

/**
 * @brief Confronto SemVer semplificato ("1.2.3", con o senza "v" iniziale).
 * @return <0 se a<b, 0 se uguali, >0 se a>b.
 */
int compareVersions(const String& a, const String& b) {
    int pa[3] = {0, 0, 0};
    int pb[3] = {0, 0, 0};
    sscanf(a.c_str() + (a.startsWith("v") ? 1 : 0), "%d.%d.%d", &pa[0],
           &pa[1], &pa[2]);
    sscanf(b.c_str() + (b.startsWith("v") ? 1 : 0), "%d.%d.%d", &pb[0],
           &pb[1], &pb[2]);
    for (int i = 0; i < 3; ++i) {
        if (pa[i] != pb[i]) {
            return pa[i] - pb[i];
        }
    }
    return 0;
}
}  // namespace

uint32_t OtaManager::crc32(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (-(crc & 1)));
        }
    }
    return ~crc;
}

void OtaManager::begin(const NodeOtaConfig& cfg, LoraManager& lora,
                       NodeManager& nodes) {
    cfg_ = cfg;
    lora_ = &lora;
    nodes_ = &nodes;
}

const char* OtaManager::stateName() const {
    switch (state_) {
        case OtaState::Idle:         return "idle";
        case OtaState::Checking:     return "checking";
        case OtaState::UpToDate:     return "up_to_date";
        case OtaState::Available:    return "available";
        case OtaState::Downloading:  return "downloading";
        case OtaState::Ready:        return "ready";
        case OtaState::Starting:     return "starting";
        case OtaState::Transferring: return "transferring";
        case OtaState::Ending:       return "ending";
        case OtaState::Success:      return "success";
        case OtaState::Error:        return "error";
    }
    return "?";
}

int OtaManager::progressPct() const {
    if (totalChunks_ == 0) {
        return 0;
    }
    return static_cast<int>(nextChunk_) * 100 / totalChunks_;
}

String OtaManager::statusJson() const {
    JsonDocument doc;
    doc["state"] = stateName();
    doc["latest_version"] = latestVersion_;
    if (targetNode_.length() > 0) {
        doc["node"] = targetNode_;
        doc["progress"] = progressPct();
    }
    if (lastError_.length() > 0) {
        doc["error"] = lastError_;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

void OtaManager::setState(OtaState s) {
    if (state_ == s) {
        return;
    }
    state_ = s;
    Logger::info(kTag, "Stato: %s", stateName());
    if (onStatus_) {
        onStatus_(statusJson());
    }
}

void OtaManager::checkNow() { checkScheduledMs_ = 0; everChecked_ = false; }

void OtaManager::abort(const char* reason) {
    lastError_ = reason;
    Logger::warn(kTag, "OTA interrotto: %s", reason);
    setState(OtaState::Error);
}

void OtaManager::loop(bool networkUp) {
    // --- Consegna risultati dei task asincroni -----------------------------
    if (checkDone_.load()) {
        checkDone_.store(false);
        if (!taskOk_) {
            lastError_ = "controllo release fallito";
            setState(OtaState::Error);
        } else {
            // C'è almeno un nodo con firmware più vecchio della release?
            bool updateNeeded = false;
            for (const NodeInfo& n : nodes_->nodes()) {
                if (n.fw.length() > 0 &&
                    compareVersions(latestVersion_, n.fw) > 0) {
                    updateNeeded = true;
                    break;
                }
            }
            Logger::info(kTag, "Ultima release: %s (%s)",
                         latestVersion_.c_str(),
                         updateNeeded ? "aggiornamento disponibile"
                                      : "nodi aggiornati");
            setState(updateNeeded ? OtaState::Available : OtaState::UpToDate);
        }
    }
    if (downloadDone_.load()) {
        downloadDone_.store(false);
        if (!taskOk_) {
            lastError_ = "download firmware fallito";
            setState(OtaState::Error);
        } else {
            totalChunks_ = static_cast<uint16_t>(
                (fwSize_ + kChunkPayload - 1) / kChunkPayload);
            Logger::info(kTag, "Firmware pronto: %u byte, %u blocchi, CRC %08lx",
                         static_cast<unsigned>(fwSize_), totalChunks_,
                         static_cast<unsigned long>(fwCrc_));
            setState(OtaState::Ready);
            startTransfer();
        }
    }

    // --- Controllo periodico -----------------------------------------------
    if (cfg_.enabled && networkUp && !taskRunning_.load() &&
        (state_ == OtaState::Idle || state_ == OtaState::UpToDate ||
         state_ == OtaState::Available || state_ == OtaState::Error ||
         state_ == OtaState::Success)) {
        const bool due = !everChecked_ ||
                         millis() - checkScheduledMs_ >
                             cfg_.checkInterval * 1000UL;
        if (due) {
            everChecked_ = true;
            checkScheduledMs_ = millis();
            taskRunning_.store(true);
            setState(OtaState::Checking);
            if (xTaskCreatePinnedToCore(checkTaskEntry, "ota_check",
                                        kTaskStack, this, 1, nullptr,
                                        0) != pdPASS) {
                taskRunning_.store(false);
                abort("impossibile creare il task di check");
            }
        }
    }

    // --- Macchina a stati del trasferimento LoRa ---------------------------
    switch (state_) {
        case OtaState::Starting:
        case OtaState::Ending:
            if (millis() - lastActionMs_ > kAckTimeoutMs) {
                if (++retries_ > kMaxRetries) {
                    abort("nessuna risposta dal nodo");
                } else {
                    Logger::warn(kTag, "Timeout, ritento (%u/%u)", retries_,
                                 kMaxRetries);
                    state_ == OtaState::Starting ? sendStart() : sendEnd();
                }
            }
            break;

        case OtaState::Transferring:
            if (windowSent_ < kWindowSize &&
                nextChunk_ + windowSent_ < totalChunks_) {
                // Invia il prossimo blocco della finestra quando la radio
                // è libera.
                if (!lora_->isBusy()) {
                    sendChunk(nextChunk_ + windowSent_);
                    ++windowSent_;
                    lastActionMs_ = millis();
                }
            } else if (millis() - lastActionMs_ > kAckTimeoutMs) {
                // Finestra completata ma nessun ACK: ritrasmetti.
                if (++retries_ > kMaxRetries) {
                    abort("ACK mancante durante il trasferimento");
                } else {
                    Logger::warn(kTag,
                                 "ACK mancante, ritrasmetto dal blocco %u "
                                 "(%u/%u)",
                                 nextChunk_, retries_, kMaxRetries);
                    windowSent_ = 0;
                }
            }
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Conferma utente e trasferimento
// ---------------------------------------------------------------------------

void OtaManager::confirm(const String& nodeId) {
    if (state_ != OtaState::Available && state_ != OtaState::Ready &&
        state_ != OtaState::Error) {
        Logger::warn(kTag, "Conferma ignorata nello stato %s", stateName());
        return;
    }
    if (nodes_->find(nodeId) == nullptr) {
        Logger::warn(kTag, "Nodo %s sconosciuto", nodeId.c_str());
        return;
    }
    targetNode_ = nodeId;
    lastError_ = "";
    retries_ = 0;

    if (fwSize_ > 0 && LittleFS.exists(kFwFile) &&
        state_ != OtaState::Available) {
        // Firmware già scaricato in questa sessione.
        totalChunks_ = static_cast<uint16_t>(
            (fwSize_ + kChunkPayload - 1) / kChunkPayload);
        startTransfer();
        return;
    }
    if (assetUrl_.isEmpty()) {
        abort("nessun asset firmware nella release");
        return;
    }
    taskRunning_.store(true);
    setState(OtaState::Downloading);
    if (xTaskCreatePinnedToCore(downloadTaskEntry, "ota_dl", kTaskStack, this,
                                1, nullptr, 0) != pdPASS) {
        taskRunning_.store(false);
        abort("impossibile creare il task di download");
    }
}

void OtaManager::startTransfer() {
    nextChunk_ = 0;
    windowSent_ = 0;
    retries_ = 0;
    sendStart();
}

void OtaManager::sendStart() {
    JsonDocument doc;
    doc["type"] = "ota_start";
    doc["id"] = targetNode_;
    doc["ver"] = latestVersion_;
    doc["size"] = static_cast<uint32_t>(fwSize_);
    doc["crc"] = fwCrc_;
    doc["chunks"] = totalChunks_;
    doc["cs"] = kChunkPayload;
    doc["win"] = kWindowSize;
    String msg;
    serializeJson(doc, msg);
    lora_->send(msg);
    lastActionMs_ = millis();
    setState(OtaState::Starting);
}

void OtaManager::sendChunk(uint16_t idx) {
    File f = LittleFS.open(kFwFile, "r");
    if (!f) {
        abort("firmware non leggibile da LittleFS");
        return;
    }
    uint8_t frame[5 + kChunkPayload];
    frame[0] = 0xFA;  // magic
    frame[1] = 0x50;
    frame[2] = idx & 0xFF;
    frame[3] = (idx >> 8) & 0xFF;
    f.seek(static_cast<uint32_t>(idx) * kChunkPayload);
    const size_t n = f.read(&frame[5], kChunkPayload);
    f.close();
    frame[4] = static_cast<uint8_t>(n);
    lora_->send(frame, 5 + n);
    Logger::debug(kTag, "Blocco %u/%u inviato (%u byte)", idx + 1,
                  totalChunks_, static_cast<unsigned>(n));
}

void OtaManager::sendEnd() {
    JsonDocument doc;
    doc["type"] = "ota_end";
    doc["id"] = targetNode_;
    doc["crc"] = fwCrc_;
    String msg;
    serializeJson(doc, msg);
    lora_->send(msg);
    lastActionMs_ = millis();
    setState(OtaState::Ending);
}

void OtaManager::handleNodeMessage(JsonDocument& doc) {
    const char* type = doc["type"] | "";
    const char* id = doc["id"] | "";
    if (targetNode_ != id) {
        return;
    }

    if (strcmp(type, "ota_ack") == 0) {
        const uint16_t next = doc["next"] | 0;
        retries_ = 0;
        lastActionMs_ = millis();
        if (next >= totalChunks_) {
            // Tutti i blocchi ricevuti: chiudi la procedura.
            nextChunk_ = totalChunks_;
            sendEnd();
            return;
        }
        nextChunk_ = next;  // supporta anche la ripresa da interruzione
        windowSent_ = 0;
        if (onStatus_) {
            onStatus_(statusJson());
        }
        setState(OtaState::Transferring);
    } else if (strcmp(type, "ota_done") == 0) {
        const bool ok = doc["ok"] | false;
        if (ok) {
            Logger::info(kTag, "Nodo %s aggiornato alla %s", id,
                         latestVersion_.c_str());
            setState(OtaState::Success);
        } else {
            abort("il nodo ha rifiutato il firmware (CRC?)");
        }
        targetNode_ = "";
    }
}

// ---------------------------------------------------------------------------
// Task asincroni (GitHub)
// ---------------------------------------------------------------------------

void OtaManager::checkTaskEntry(void* self) {
    auto* mgr = static_cast<OtaManager*>(self);
    mgr->runCheck();
    mgr->taskRunning_.store(false);
    mgr->checkDone_.store(true);
    vTaskDelete(nullptr);
}

void OtaManager::downloadTaskEntry(void* self) {
    auto* mgr = static_cast<OtaManager*>(self);
    mgr->runDownload();
    mgr->taskRunning_.store(false);
    mgr->downloadDone_.store(true);
    vTaskDelete(nullptr);
}

void OtaManager::runCheck() {
    taskOk_ = false;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);

    const String url =
        "https://api.github.com/repos/" + cfg_.repo + "/releases/latest";
    if (!http.begin(client, url)) {
        return;
    }
    http.addHeader("User-Agent", String(fw::kName) + "/" + fw::kVersion);
    http.addHeader("Accept", "application/vnd.github+json");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Logger::warn(kTag, "GitHub API: HTTP %d", code);
        http.end();
        return;
    }

    JsonDocument filter;
    filter["tag_name"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;
    filter["assets"][0]["size"] = true;

    JsonDocument doc;
    const DeserializationError err = deserializeJson(
        doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err) {
        Logger::warn(kTag, "Risposta GitHub non valida: %s", err.c_str());
        return;
    }

    latestVersion_ = doc["tag_name"] | "";
    assetUrl_ = "";
    assetSize_ = 0;
    for (JsonObjectConst asset : doc["assets"].as<JsonArrayConst>()) {
        const char* name = asset["name"] | "";
        if (cfg_.assetName == name) {
            assetUrl_ = asset["browser_download_url"] | "";
            assetSize_ = asset["size"] | 0;
            break;
        }
    }
    taskOk_ = latestVersion_.length() > 0;
}

void OtaManager::runDownload() {
    taskOk_ = false;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(30000);
    // I download degli asset GitHub passano da redirect su un CDN.
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (!http.begin(client, assetUrl_)) {
        return;
    }
    http.addHeader("User-Agent", String(fw::kName) + "/" + fw::kVersion);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Logger::warn(kTag, "Download firmware: HTTP %d", code);
        http.end();
        return;
    }

    File f = LittleFS.open(kFwFile, "w");
    if (!f) {
        http.end();
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint32_t crc = 0;
    size_t total = 0;
    uint8_t buf[1024];
    int remaining = http.getSize();  // -1 se chunked
    while (http.connected() && (remaining > 0 || remaining == -1)) {
        const size_t avail = stream->available();
        if (avail == 0) {
            delay(10);
            if (!stream->connected()) {
                break;
            }
            continue;
        }
        const size_t n =
            stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
        if (n == 0) {
            break;
        }
        f.write(buf, n);
        crc = crc32(crc, buf, n);
        total += n;
        if (remaining > 0) {
            remaining -= n;
        }
    }
    f.close();
    http.end();

    if (total == 0 || (assetSize_ > 0 && total != assetSize_)) {
        Logger::warn(kTag, "Download incompleto (%u/%u byte)",
                     static_cast<unsigned>(total),
                     static_cast<unsigned>(assetSize_));
        return;
    }
    fwSize_ = total;
    fwCrc_ = crc;
    taskOk_ = true;
}
