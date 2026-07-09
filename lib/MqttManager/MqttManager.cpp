/**
 * @file MqttManager.cpp
 * @brief Implementazione del client MQTT.
 */
#include "MqttManager.h"

#include <WiFi.h>

namespace {
constexpr const char* kTag = "MQTT";
}

void MqttManager::begin(const MqttConfig& cfg, const String& gatewayName) {
    cfg_ = cfg;
    name_ = gatewayName;
    topicPrefix_ = cfg_.baseTopic + "/gateway/" + name_;
    client_.setClient(net_);
    client_.setServer(cfg_.host.c_str(), cfg_.port);
    client_.setBufferSize(kBufferSize);
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
    String clientId = name_ + "-" + String(static_cast<uint32_t>(
                                        ESP.getEfuseMac() & 0xFFFFFF), HEX);
    const String willTopic = topicPrefix_ + "/status";
    Logger::info(kTag, "Connessione a %s:%u...", cfg_.host.c_str(),
                 cfg_.port);
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
        Logger::warn(kTag, "Connessione fallita (stato %d)", client_.state());
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
