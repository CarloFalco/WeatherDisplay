/**
 * @file MqttManager.cpp
 * @brief Implementazione del client MQTT.
 */
#include "MqttManager.h"

#include <WiFi.h>

namespace {
constexpr const char* kTag = "MQTT";

/** @brief Traduce il codice di stato di PubSubClient in testo. */
const char* stateText(int state) {
    switch (state) {
        case -4: return "timeout";
        case -3: return "connessione persa";
        case -2: return "connessione rifiutata (rete/TLS?)";
        case -1: return "disconnesso";
        case 1:  return "protocollo non supportato";
        case 2:  return "client id rifiutato";
        case 3:  return "broker non disponibile";
        case 4:  return "utente o password errati";
        case 5:  return "non autorizzato";
        default: return "sconosciuto";
    }
}
}  // namespace

void MqttManager::begin(const MqttConfig& cfg, const String& gatewayName) {
    cfg_ = cfg;
    name_ = gatewayName;
    topicPrefix_ = cfg_.baseTopic + "/gateway/" + name_;
    if (cfg_.useTls()) {
        if (caCert_ != nullptr) {
            // Verifica il certificato del broker contro la CA fornita
            // (es. Let's Encrypt ISRG Root X1).
            netSecure_.setCACert(caCert_);
            Logger::info(kTag, "MQTT in TLS con verifica certificato");
        } else {
            // Nessuna CA: traffico cifrato ma broker non autenticato.
            netSecure_.setInsecure();
            Logger::info(kTag, "MQTT in TLS senza verifica certificato");
        }
        // Limita la durata di un tentativo fallito: l'handshake gira nel
        // loop principale (default 120s = loop congelato a broker muto).
        netSecure_.setHandshakeTimeout(10);
        client_.setClient(netSecure_);
    } else {
        client_.setClient(net_);
    }
    client_.setServer(cfg_.host.c_str(), cfg_.port);
    client_.setBufferSize(kBufferSize);
    client_.setSocketTimeout(8);  // secondi, default 15
    client_.setCallback(
        [this](char* topic, uint8_t* payload, unsigned int length) {
            handleMessage(topic, payload, length);
        });
}

void MqttManager::loop(bool networkUp) {
    if (cfg_.host.isEmpty()) {
        return;  // MQTT non configurato
    }
    if (!networkUp) {
        wasConnected_ = client_.connected();
        return;
    }
    if (!client_.connected()) {
        if (wasConnected_) {
            Logger::warn(kTag, "Connessione al broker persa");
            wasConnected_ = false;
        }
        if (millis() - lastAttemptMs_ > kRetryIntervalMs) {
            lastAttemptMs_ = millis();
            connect();
        }
        return;
    }
    wasConnected_ = true;
    client_.loop();
}

void MqttManager::connect() {
    String clientId =
        cfg_.clientId.length() > 0
            ? cfg_.clientId
            : name_ + "-" + String(static_cast<uint32_t>(
                                       ESP.getEfuseMac() & 0xFFFFFF),
                                   HEX);
    const String willTopic = topicPrefix_ + "/status";
    Logger::info(kTag, "Connessione a %s:%u%s...", cfg_.host.c_str(),
                 cfg_.port, cfg_.useTls() ? " (TLS)" : "");
    bool ok;
    if (cfg_.username.length() > 0) {
        ok = client_.connect(clientId.c_str(), cfg_.username.c_str(),
                             cfg_.password.c_str(), willTopic.c_str(), 1,
                             true, "offline");
    } else {
        ok = client_.connect(clientId.c_str(), willTopic.c_str(), 1, true,
                             "offline");
    }
    if (!ok) {
        const int st = client_.state();
        Logger::warn(kTag, "Connessione fallita: %s (stato %d)",
                     stateText(st), st);
        return;
    }
    Logger::info(kTag, "Connesso al broker");
    client_.publish(willTopic.c_str(), "online", true);
    const String cmdTopic = topicPrefix_ + "/cmd/#";
    client_.subscribe(cmdTopic.c_str(), 1);
    wasConnected_ = true;
}

void MqttManager::handleMessage(char* topic, uint8_t* payload,
                                unsigned int length) {
    String t(topic);
    const String cmdPrefix = topicPrefix_ + "/cmd/";
    if (!t.startsWith(cmdPrefix)) {
        return;
    }
    const String cmd = t.substring(cmdPrefix.length());
    String body;
    body.reserve(length);
    for (unsigned int i = 0; i < length; ++i) {
        body += static_cast<char>(payload[i]);
    }
    Logger::info(kTag, "Comando ricevuto: %s", cmd.c_str());
    if (onCommand_) {
        onCommand_(cmd, body);
    }
}

bool MqttManager::publish(const String& topic, const String& payload,
                          bool retained) {
    if (!client_.connected()) {
        return false;
    }
    const bool ok =
        client_.publish(topic.c_str(), payload.c_str(), retained);
    if (ok) {
        ++publishCount_;
    }
    return ok;
}

void MqttManager::publishNodeData(const String& nodeId, const String& json) {
    publish(cfg_.baseTopic + "/node/" + nodeId + "/data", json, false);
}

void MqttManager::publishGatewayInfo(const String& json) {
    publish(topicPrefix_ + "/info", json, true);
}

void MqttManager::publishLog(LogLevel level, const char* line) {
    // Nessun log dentro questa funzione: è il sink del logger stesso.
    (void)level;
    if (!client_.connected()) {
        return;
    }
    client_.publish((topicPrefix_ + "/log").c_str(), line, false);
}

void MqttManager::publishOtaStatus(const String& json) {
    publish(topicPrefix_ + "/ota", json, true);
}
