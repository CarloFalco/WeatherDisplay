/**
 * @file WebPortal.h
 * @brief Interfaccia Web di configurazione e monitoraggio.
 *
 * Serve una singola pagina (embedded in flash) che permette di:
 *  - vedere stato Wi-Fi/MQTT/LoRa, nodi collegati e versione firmware
 *  - visualizzare e modificare l'intera configurazione
 *  - esportare/importare la configurazione
 *  - riavviare il gateway
 *  - avviare il controllo/aggiornamento OTA dei nodi
 *  - aggiornare il firmware del gateway (upload .bin)
 *
 * In modalità Access Point funge anche da captive portal.
 */
#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <functional>

#include "ConfigManager.h"
#include "LoraManager.h"
#include "MqttManager.h"
#include "NodeManager.h"
#include "OtaManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"
#include "WifiManager.h"

/**
 * @brief Server HTTP di configurazione (porta 80).
 */
class WebPortal {
   public:
    /** @brief Richiesta di riavvio (gestita da main con ritardo). */
    using RebootFn = std::function<void()>;

    WebPortal(ConfigManager& config, WifiManager& wifi, MqttManager& mqtt,
              LoraManager& lora, NodeManager& nodes, WeatherManager& weather,
              OtaManager& ota, TimeManager& time)
        : config_(config),
          wifi_(wifi),
          mqtt_(mqtt),
          lora_(lora),
          nodes_(nodes),
          weather_(weather),
          ota_(ota),
          time_(time),
          server_(80) {}

    void begin();
    void loop();

    void onReboot(RebootFn cb) { reboot_ = cb; }

   private:
    void handleRoot();
    void handleStatus();
    void handleGetConfig(bool withSecrets);
    void handleSetConfig();
    void handleReboot();
    void handleFactoryReset();
    void handleOtaCheck();
    void handleOtaConfirm();
    void handleFwUpload();
    void handleFwUploadDone();
    void handleNotFound();

    ConfigManager& config_;
    WifiManager& wifi_;
    MqttManager& mqtt_;
    LoraManager& lora_;
    NodeManager& nodes_;
    WeatherManager& weather_;
    OtaManager& ota_;
    TimeManager& time_;
    WebServer server_;
    RebootFn reboot_;
    bool uploadOk_ = false;
};
