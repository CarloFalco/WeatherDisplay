/**
 * @file WebPortal.cpp
 * @brief Implementazione della Web UI.
 */
#include "WebPortal.h"

#include <ArduinoJson.h>
#include <Logger.h>
#include <Update.h>

#include "../../include/version.h"
#include "WebPages.h"

namespace {
constexpr const char* kTag = "Web";

String formatUptime(uint32_t ms) {
    const uint32_t s = ms / 1000;
    char buf[24];
    snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu",
             static_cast<unsigned long>(s / 86400),
             static_cast<unsigned long>((s / 3600) % 24),
             static_cast<unsigned long>((s / 60) % 60),
             static_cast<unsigned long>(s % 60));
    return buf;
}
}  // namespace

void WebPortal::begin() {
    server_.on("/", HTTP_GET, [this] { handleRoot(); });
    server_.on("/api/status", HTTP_GET, [this] { handleStatus(); });
    server_.on("/api/config", HTTP_GET, [this] { handleGetConfig(false); });
    server_.on("/api/config/export", HTTP_GET,
               [this] { handleGetConfig(true); });
    server_.on("/api/config", HTTP_POST, [this] { handleSetConfig(); });
    server_.on("/api/config/import", HTTP_POST,
               [this] { handleSetConfig(); });
    server_.on("/api/reboot", HTTP_POST, [this] { handleReboot(); });
    server_.on("/api/factory-reset", HTTP_POST,
               [this] { handleFactoryReset(); });
    server_.on("/api/ota/check", HTTP_POST, [this] { handleOtaCheck(); });
    server_.on("/api/ota/confirm", HTTP_POST,
               [this] { handleOtaConfirm(); });
    server_.on(
        "/update", HTTP_POST, [this] { handleFwUploadDone(); },
        [this] { handleFwUpload(); });
    server_.onNotFound([this] { handleNotFound(); });
    server_.begin();
    Logger::info(kTag, "Web server avviato sulla porta 80");
}

void WebPortal::loop() { server_.handleClient(); }

void WebPortal::handleRoot() {
    server_.send_P(200, "text/html", kIndexHtml);
}

void WebPortal::handleNotFound() {
    if (wifi_.isApMode()) {
        // Captive portal: reindirizza tutto alla pagina di configurazione.
        server_.sendHeader("Location", "http://" + wifi_.ip() + "/", true);
        server_.send(302, "text/plain", "");
        return;
    }
    server_.send(404, "text/plain", "Not found");
}

void WebPortal::handleStatus() {
    JsonDocument doc;
    doc["fw"] = fw::kVersion;
    doc["name"] = config_.get().gateway.name;
    doc["uptime"] = formatUptime(millis());
    doc["free_heap"] = ESP.getFreeHeap();
    doc["time"] = time_.isSynced()
                      ? time_.dateitalian() + " " + time_.timeShort()
                      : String("non sincronizzata");

    JsonObject jw = doc["wifi"].to<JsonObject>();
    jw["connected"] = wifi_.isConnected();
    jw["ap_mode"] = wifi_.isApMode();
    jw["ssid"] = wifi_.isApMode() ? wifi_.apSsid() : config_.get().wifi.ssid;
    jw["ip"] = wifi_.ip();
    jw["rssi"] = wifi_.rssi();

    doc["mqtt"]["connected"] = mqtt_.isConnected();

    JsonObject jl = doc["lora"].to<JsonObject>();
    jl["ready"] = lora_.isReady();
    jl["packets"] = lora_.packetsReceived();
    jl["rssi"] = lora_.lastRssi();

    JsonObject jm = doc["weather"].to<JsonObject>();
    jm["valid"] = weather_.current().valid;
    if (weather_.current().valid) {
        jm["temp"] = serialized(String(weather_.current().temp, 1));
        jm["desc"] = weather_.current().description;
    }

    JsonArray jn = doc["nodes"].to<JsonArray>();
    for (const NodeInfo& n : nodes_.nodes()) {
        JsonObject o = jn.add<JsonObject>();
        o["id"] = n.id;
        o["fw"] = n.fw;
        if (!isnan(n.t)) o["t"] = serialized(String(n.t, 1));
        if (!isnan(n.rh)) o["rh"] = serialized(String(n.rh, 1));
        if (!isnan(n.vbat)) {
            o["vbat"] = serialized(String(n.vbat, 2));
            o["battery_pct"] = n.batteryPct();
        }
        o["rssi"] = serialized(String(n.rssi, 0));
        o["last_seen"] = n.lastSeenEpoch > 0
                             ? time_.timeShortFrom(n.lastSeenEpoch)
                             : String("-");
        o["offline"] = n.offline;
    }

    JsonObject jo = doc["ota"].to<JsonObject>();
    jo["state"] = ota_.stateName();
    jo["latest_version"] = ota_.latestVersion();
    if (ota_.inProgress()) {
        jo["progress"] = ota_.progressPct();
    }

    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
}

void WebPortal::handleGetConfig(bool withSecrets) {
    JsonDocument doc;
    config_.toJson(doc, withSecrets);
    String out;
    serializeJsonPretty(doc, out);
    server_.send(200, "application/json", out);
}

void WebPortal::handleSetConfig() {
    JsonDocument doc;
    const DeserializationError err =
        deserializeJson(doc, server_.arg("plain"));
    if (err) {
        server_.send(400, "text/plain",
                     String("JSON non valido: ") + err.c_str());
        return;
    }
    config_.fromJson(doc);
    if (!config_.save()) {
        server_.send(500, "text/plain", "Salvataggio fallito");
        return;
    }
    server_.send(200, "text/plain", "OK");
    Logger::info(kTag, "Configurazione aggiornata dalla Web UI, riavvio");
    if (reboot_) {
        reboot_();
    }
}

void WebPortal::handleReboot() {
    server_.send(200, "text/plain", "OK");
    if (reboot_) {
        reboot_();
    }
}

void WebPortal::handleFactoryReset() {
    const bool ok = config_.factoryReset();
    if (!ok) {
        server_.send(500, "text/plain",
                     "Cancellazione dati parzialmente fallita");
        return;
    }
    server_.send(200, "text/plain", "OK");
    Logger::warn(kTag, "Ripristino di fabbrica dalla Web UI, riavvio");
    if (reboot_) {
        reboot_();
    }
}

void WebPortal::handleOtaCheck() {
    ota_.checkNow();
    server_.send(200, "text/plain", "OK");
}

void WebPortal::handleOtaConfirm() {
    String nodeId = server_.arg("plain");
    nodeId.trim();
    if (nodeId.isEmpty()) {
        server_.send(400, "text/plain", "Indicare l'id del nodo");
        return;
    }
    ota_.confirm(nodeId);
    server_.send(200, "text/plain", "OK");
}

void WebPortal::handleFwUpload() {
    // Aggiornamento firmware del gateway: scrive solo la partizione app,
    // la configurazione su LittleFS resta intatta.
    HTTPUpload& up = server_.upload();
    if (up.status == UPLOAD_FILE_START) {
        Logger::info(kTag, "Aggiornamento firmware gateway: %s",
                     up.filename.c_str());
        uploadOk_ = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        if (!uploadOk_) {
            Logger::error(kTag, "Update.begin fallita");
        }
    } else if (up.status == UPLOAD_FILE_WRITE && uploadOk_) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            uploadOk_ = false;
            Logger::error(kTag, "Scrittura update fallita");
        }
    } else if (up.status == UPLOAD_FILE_END && uploadOk_) {
        uploadOk_ = Update.end(true);
        Logger::info(kTag, uploadOk_ ? "Firmware scritto (%u byte)"
                                     : "Update.end fallita (%u byte)",
                     up.totalSize);
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        uploadOk_ = false;
    }
}

void WebPortal::handleFwUploadDone() {
    if (uploadOk_) {
        server_.send(200, "text/html",
                     "Aggiornamento riuscito, riavvio in corso... <a "
                     "href='/'>torna alla home</a>");
        if (reboot_) {
            reboot_();
        }
    } else {
        server_.send(500, "text/plain", "Aggiornamento fallito");
    }
}
