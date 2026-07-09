/**
 * @file MqttManager.h
 * @brief Pubblicazione dati e comandi remoti via MQTT.
 *
 * Struttura dei topic (base = mqtt.base_topic, name = gateway.name):
 *   {base}/gateway/{name}/status   online/offline (retained, LWT)
 *   {base}/gateway/{name}/info     stato del gateway (retained)
 *   {base}/gateway/{name}/log      log del firmware (se abilitato)
 *   {base}/gateway/{name}/ota      stato OTA nodi
 *   {base}/node/{id}/data          telemetria delle stazioni
 *   {base}/gateway/{name}/cmd/+    comandi in ingresso
 *
 * Comandi supportati: restart, refresh, info, ota_check, ota_confirm.
 */
#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include <functional>

#include "ConfigManager.h"
#include "Logger.h"

/**
 * @brief Client MQTT con riconnessione automatica non bloccante.
 */
class MqttManager {
   public:
    /** @brief Callback comando: nome (ultimo livello del topic) e payload. */
    using CommandFn =
        std::function<void(const String& cmd, const String& payload)>;

    void begin(const MqttConfig& cfg, const String& gatewayName);

    /** @brief Da chiamare a ogni giro di loop(). */
    void loop(bool networkUp);

    bool isConnected() { return client_.connected(); }

    /** @brief Telemetria stazione su {base}/node/{id}/data. */
    void publishNodeData(const String& nodeId, const String& json);

    /** @brief Stato gateway (retained) su {base}/gateway/{name}/info. */
    void publishGatewayInfo(const String& json);

    /** @brief Riga di log su {base}/gateway/{name}/log. */
    void publishLog(LogLevel level, const char* line);

    /** @brief Stato del processo OTA su {base}/gateway/{name}/ota. */
    void publishOtaStatus(const String& json);

    void onCommand(CommandFn cb) { onCommand_ = cb; }

    /** @brief Numero di pubblicazioni riuscite (diagnostica). */
    uint32_t publishCount() const { return publishCount_; }

   private:
    static constexpr uint32_t kRetryIntervalMs = 5000;
    static constexpr uint16_t kBufferSize = 1024;

    void connect();
    void handleMessage(char* topic, uint8_t* payload, unsigned int length);
    bool publish(const String& topic, const String& payload, bool retained);

    MqttConfig cfg_;
    String name_;
    String topicPrefix_;  ///< "{base}/gateway/{name}"
    WiFiClient net_;
    PubSubClient client_;
    uint32_t lastAttemptMs_ = 0;
    uint32_t publishCount_ = 0;
    bool wasConnected_ = false;
    CommandFn onCommand_;
};
