/**
 * @file NodeManager.cpp
 * @brief Implementazione della gestione stazioni.
 */
#include "NodeManager.h"

#include <Logger.h>

#include <cmath>

namespace {
constexpr const char* kTag = "Node";

/** @brief Legge un float dal JSON; NAN se assente. */
float floatOr(JsonVariantConst v, float fallback) {
    return v.isNull() ? fallback : v.as<float>();
}
}  // namespace

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

History::History() {
    for (size_t i = 0; i < kHistorySlots; ++i) {
        temp[i] = NAN;
        rh[i] = NAN;
        press[i] = NAN;
        rain[i] = NAN;
        slotEpoch[i] = 0;
    }
}

void History::addSample(time_t now, float t, float h, float p,
                        float rainDelta) {
    if (now <= 0) {
        return;  // senza ora valida lo storico non è collocabile
    }
    const uint32_t slot = static_cast<uint32_t>(now / kHistorySlotSeconds);
    if (slotEpoch[head] != slot) {
        // Nuovo slot temporale.
        head = (head + 1) % kHistorySlots;
        slotEpoch[head] = slot;
        temp[head] = NAN;
        rh[head] = NAN;
        press[head] = NAN;
        rain[head] = NAN;
    }
    // Ultimo valore vince per le grandezze istantanee; la pioggia si somma.
    if (!isnan(t)) temp[head] = t;
    if (!isnan(h)) rh[head] = h;
    if (!isnan(p)) press[head] = p;
    if (!isnan(rainDelta) && rainDelta > 0) {
        rain[head] = isnan(rain[head]) ? rainDelta : rain[head] + rainDelta;
    }
}

void History::series(int which, float* out) const {
    for (size_t i = 0; i < kHistorySlots; ++i) {
        const size_t idx = (head + 1 + i) % kHistorySlots;
        if (slotEpoch[idx] == 0) {
            out[i] = NAN;
            continue;
        }
        switch (which) {
            case 0: out[i] = temp[idx]; break;
            case 1: out[i] = rh[idx]; break;
            case 2: out[i] = press[idx]; break;
            default: out[i] = rain[idx]; break;
        }
    }
}

// ---------------------------------------------------------------------------
// NodeInfo
// ---------------------------------------------------------------------------

int NodeInfo::batteryPct() const {
    if (isnan(vbat)) {
        return -1;
    }
    // Curva LiPo approssimata a tratti (a riposo).
    struct Point { float v; int pct; };
    constexpr Point kCurve[] = {
        {4.20f, 100}, {4.10f, 90}, {4.00f, 78}, {3.90f, 62}, {3.80f, 45},
        {3.70f, 28},  {3.60f, 15}, {3.50f, 8},  {3.40f, 4},  {3.30f, 0},
    };
    if (vbat >= kCurve[0].v) return 100;
    for (size_t i = 1; i < sizeof(kCurve) / sizeof(kCurve[0]); ++i) {
        if (vbat >= kCurve[i].v) {
            const Point& a = kCurve[i];
            const Point& b = kCurve[i - 1];
            return a.pct + static_cast<int>((vbat - a.v) / (b.v - a.v) *
                                            (b.pct - a.pct));
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// NodeManager
// ---------------------------------------------------------------------------

bool NodeManager::handlePacket(const uint8_t* data, size_t len, float rssi,
                               float snr, time_t nowEpoch) {
    // I pacchetti del protocollo sono JSON; ignora silenziosamente altro
    // traffico sullo stesso canale.
    if (len < 2 || data[0] != '{') {
        Logger::debug(kTag, "Pacchetto non JSON ignorato");
        return false;
    }

    JsonDocument doc;
    const DeserializationError err =
        deserializeJson(doc, reinterpret_cast<const char*>(data), len);
    if (err) {
        Logger::warn(kTag, "JSON non valido: %s", err.c_str());
        return false;
    }

    const char* type = doc["type"] | "";
    const char* id = doc["id"] | "";
    if (id[0] == '\0') {
        Logger::warn(kTag, "Pacchetto senza id, scartato");
        return false;
    }

    if (strncmp(type, "ota", 3) == 0) {
        // Messaggi del protocollo OTA: inoltra a OtaManager.
        if (onOtaMsg_) {
            onOtaMsg_(doc);
        }
        return true;
    }

    if (strcmp(type, "data") != 0) {
        Logger::debug(kTag, "Tipo \"%s\" non gestito", type);
        return false;
    }

    NodeInfo& node = findOrCreate(id);
    const uint16_t seq = doc["seq"] | 0;

    // Rileva pacchetti persi dal contatore progressivo.
    if (node.packetsReceived > 0) {
        const uint16_t expected = static_cast<uint16_t>(node.seq + 1);
        if (seq != expected && seq > expected) {
            node.packetsLost += seq - expected;
        }
    }
    node.seq = seq;
    ++node.packetsReceived;
    node.lastSeenMs = millis();
    node.lastSeenEpoch = nowEpoch;
    node.rssi = rssi;
    node.snr = snr;
    if (!doc["fw"].isNull()) {
        node.fw = doc["fw"].as<const char*>();
    }

    // Telemetria: i campi assenti mantengono il valore precedente,
    // tranne quelli "evento" (rain) che vanno considerati solo se presenti.
    node.t = floatOr(doc["t"], node.t);
    node.rh = floatOr(doc["rh"], node.rh);
    node.p = floatOr(doc["p"], node.p);
    node.rain = floatOr(doc["rain"], NAN);  // delta del singolo pacchetto
    node.ws = floatOr(doc["ws"], node.ws);
    node.wg = floatOr(doc["wg"], node.wg);
    if (!doc["wd"].isNull()) {
        node.wd = doc["wd"].as<int>();
    }
    node.vbat = floatOr(doc["vbat"], node.vbat);
    node.ibat = floatOr(doc["ibat"], node.ibat);
    node.ipan = floatOr(doc["ipan"], node.ipan);
    node.iload = floatOr(doc["iload"], node.iload);

    node.history.addSample(nowEpoch, floatOr(doc["t"], NAN),
                           floatOr(doc["rh"], NAN), floatOr(doc["p"], NAN),
                           floatOr(doc["rain"], NAN));

    const bool wasOffline = node.offline;
    node.offline = false;
    if (wasOffline && onOffline_) {
        onOffline_(node, false);
    }

    Logger::info(kTag, "%s: seq=%u t=%.1f rh=%.1f p=%.1f vbat=%.2f (RSSI %.0f)",
                 node.id.c_str(), seq, node.t, node.rh, node.p, node.vbat,
                 rssi);

    if (ackSender_) {
        ackSender_(node.id, seq);
    }
    if (onData_) {
        onData_(node);
    }
    return true;
}

void NodeManager::loop() {
    const uint32_t now = millis();
    for (NodeInfo& node : nodes_) {
        if (!node.offline && node.lastSeenMs != 0 &&
            now - node.lastSeenMs > timeoutS_ * 1000UL) {
            node.offline = true;
            Logger::warn(kTag, "Nodo %s offline (nessun dato da %lus)",
                         node.id.c_str(),
                         static_cast<unsigned long>((now - node.lastSeenMs) /
                                                    1000));
            if (onOffline_) {
                onOffline_(node, true);
            }
        }
    }
}

NodeInfo* NodeManager::find(const String& id) {
    for (NodeInfo& n : nodes_) {
        if (n.id == id) {
            return &n;
        }
    }
    return nullptr;
}

NodeInfo& NodeManager::findOrCreate(const String& id) {
    NodeInfo* existing = find(id);
    if (existing != nullptr) {
        return *existing;
    }
    Logger::info(kTag, "Nuova stazione rilevata: %s", id.c_str());
    nodes_.emplace_back();
    nodes_.back().id = id;
    return nodes_.back();
}

String NodeManager::toJson(const NodeInfo& node) {
    JsonDocument doc;
    doc["id"] = node.id;
    doc["fw"] = node.fw;
    doc["seq"] = node.seq;
    doc["rssi"] = node.rssi;
    doc["snr"] = node.snr;
    doc["offline"] = node.offline;
    doc["last_seen"] = static_cast<uint32_t>(node.lastSeenEpoch);
    doc["packets_lost"] = node.packetsLost;

    auto put = [&doc](const char* key, float v) {
        if (!isnan(v)) {
            doc[key] = serialized(String(v, 1));
        }
    };
    put("t", node.t);
    put("rh", node.rh);
    put("p", node.p);
    put("rain", node.rain);
    put("ws", node.ws);
    put("wg", node.wg);
    if (node.wd >= 0) {
        doc["wd"] = node.wd;
    }
    if (!isnan(node.vbat)) {
        doc["vbat"] = serialized(String(node.vbat, 2));
        doc["battery_pct"] = node.batteryPct();
    }
    put("ibat", node.ibat);
    put("ipan", node.ipan);
    put("iload", node.iload);

    String out;
    serializeJson(doc, out);
    return out;
}
