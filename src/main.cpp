/**
 * @file main.cpp
 * @brief Gateway Meteo LoRa - LILYGO T5 4.7" ESP32-S3 E-Paper.
 *
 * Punto di ingresso: istanzia i moduli, li collega tra loro tramite
 * callback (nessuna dipendenza circolare) e li serve nel loop principale
 * senza mai bloccare.
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

#include "../include/version.h"
#include <ConfigManager.h>
#include <DisplayManager.h>
#include <Logger.h>
#include <LoraManager.h>
#include <MqttManager.h>
#include <NodeManager.h>
#include <OtaManager.h>
#include <TimeManager.h>
#include <WeatherManager.h>
#include <WebPortal.h>
#include <WifiManager.h>

#include "letsEncryptCaCrt.h"  // CA Let's Encrypt per il broker MQTT (leCaCrt)

/**
 * @brief Aumenta lo stack del task loop() da 8 KB (default) a 16 KB.
 *
 * L'handshake TLS di mbedTLS (MQTT su 8883, HTTPS) consuma più stack di
 * quello di default: senza questo aumento il primo handshake provoca uno
 * stack overflow del loopTask e un boot loop continuo.
 */
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

namespace {
constexpr const char* kTag = "Main";

/**
 * @brief Contenitore di tutti i moduli (unico oggetto a livello di file:
 *        evita lo spargimento di variabili globali).
 */
struct App {
    ConfigManager config;
    WifiManager wifi;
    TimeManager time;
    LoraManager lora;
    NodeManager nodes;
    MqttManager mqtt;
    WeatherManager weather;
    OtaManager ota;
    DisplayManager display;
    WebPortal web;

    bool configured = false;
    bool timeSyncLogged = false;  ///< log una-tantum a NTP sincronizzato
    uint32_t rebootAtMs = 0;      ///< 0 = nessun riavvio richiesto
    uint32_t lastInfoPubMs = 0;   ///< pubblicazione periodica stato gateway
    uint32_t lastNodeLoopMs = 0;

    App() : web(config, wifi, mqtt, lora, nodes, weather, ota, time) {}
};

App* app = nullptr;

/** @brief Richiede un riavvio ritardato (lascia uscire la risposta HTTP). */
void scheduleReboot() { app->rebootAtMs = millis() + 800; }

/** @brief Stato del gateway per il display. */
GatewayStatus buildStatus() {
    GatewayStatus st;
    st.wifiConnected = app->wifi.isConnected();
    st.apMode = app->wifi.isApMode();
    st.ip = app->wifi.ip();
    st.mqttConnected = app->mqtt.isConnected();
    st.mqttConfigured = !app->config.get().mqtt.host.isEmpty();
    st.loraReady = app->lora.isReady();
    st.otaInProgress = app->ota.inProgress();
    st.otaProgress = app->ota.progressPct();
    st.otaNode = "";
    st.weatherFailed =
        app->config.get().weatherApi.enabled && app->weather.lastFailed();
    return st;
}

/** @brief Pubblica lo stato del gateway su MQTT. */
void publishGatewayInfo() {
    JsonDocument doc;
    doc["fw"] = fw::kVersion;
    doc["name"] = app->config.get().gateway.name;
    doc["ip"] = app->wifi.ip();
    doc["wifi_rssi"] = app->wifi.rssi();
    doc["uptime_s"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["lora_rssi"] = app->lora.lastRssi();
    doc["lora_packets"] = app->lora.packetsReceived();
    doc["nodes"] = app->nodes.count();
    String out;
    serializeJson(doc, out);
    app->mqtt.publishGatewayInfo(out);
}

/** @brief Gestione dei comandi ricevuti via MQTT. */
void handleCommand(const String& cmd, const String& payload) {
    if (cmd == "restart") {
        Logger::info(kTag, "Riavvio richiesto via MQTT");
        scheduleReboot();
    } else if (cmd == "refresh") {
        app->display.requestRefresh();
    } else if (cmd == "info") {
        publishGatewayInfo();
    } else if (cmd == "ota_check") {
        app->ota.checkNow();
    } else if (cmd == "ota_confirm") {
        String id = payload;
        id.trim();
        app->ota.confirm(id);
    } else if (cmd == "factory_reset") {
        Logger::warn(kTag, "Ripristino di fabbrica richiesto via MQTT");
        app->config.factoryReset();
        scheduleReboot();
    } else {
        Logger::warn(kTag, "Comando sconosciuto: %s", cmd.c_str());
    }
}

/** @brief Collega i moduli tra loro (modalità normale). */
void wireModules() {
    const Config& cfg = app->config.get();

    // Wi-Fi connesso -> NTP + OTA gateway via ArduinoOTA.
    app->wifi.onConnected([] {
        app->time.startSync();
        ArduinoOTA.setHostname(app->config.get().gateway.name.c_str());
        ArduinoOTA.begin();
        app->display.markDirty();
    });

    // Pacchetto LoRa -> NodeManager.
    app->lora.onPacket(
        [](const uint8_t* data, size_t len, float rssi, float snr) {
            app->nodes.handlePacket(data, len, rssi, snr, app->time.now());
        });

    // ACK verso le stazioni.
    app->nodes.setAckSender([](const String& id, uint16_t seq) {
        JsonDocument doc;
        doc["type"] = "ack";
        doc["id"] = id;
        doc["seq"] = seq;
        String msg;
        serializeJson(doc, msg);
        app->lora.send(msg);
    });

    // Nuovi dati stazione -> MQTT + display.
    app->nodes.onData([](const NodeInfo& node) {
        app->mqtt.publishNodeData(node.id, NodeManager::toJson(node));
        app->display.markDirty();
    });
    app->nodes.onOfflineChange(
        [](const NodeInfo&, bool) { app->display.markDirty(); });

    // Messaggi OTA dei nodi -> OtaManager.
    app->nodes.onOtaMessage(
        [](JsonDocument& doc) { app->ota.handleNodeMessage(doc); });

    // Stato OTA -> MQTT + display.
    app->ota.onStatus([](const String& json) {
        app->mqtt.publishOtaStatus(json);
        app->display.markDirty();
    });

    // Meteo esterno aggiornato -> display.
    app->weather.onUpdated([] { app->display.markDirty(); });

    // Comandi MQTT.
    app->mqtt.onCommand(handleCommand);

    // Log via MQTT (se abilitato).
    if (cfg.log.mqtt) {
        Logger::setRemoteSink([](LogLevel lvl, const char* line) {
            app->mqtt.publishLog(lvl, line);
        });
    }

    // Riavvio dalla Web UI.
    app->web.onReboot(scheduleReboot);

    // Stato per il display.
    app->display.setStatusProvider(buildStatus);
}
}  // namespace

void setup() {
    Serial.begin(115200);
    Logger::begin(LogLevel::Info);
    Logger::info(kTag, "%s v%s in avvio...", fw::kName, fw::kVersion);

    app = new App();

    if (!app->config.begin()) {
        Logger::error(kTag, "LittleFS non disponibile, riavvio tra 5s");
        delay(5000);
        ESP.restart();
    }

    app->configured = app->config.exists() && app->config.load();
    const Config& cfg = app->config.get();

    // Logger secondo configurazione.
    Logger::setLevel(Logger::levelFromString(cfg.log.level.c_str()));
    Logger::setFileLogging(cfg.log.file);

    app->nodes.setTimeout(cfg.display.nodeTimeout);

    // Il display serve in entrambe le modalità.
    app->time.begin(cfg.gateway.timezone);
    app->display.begin(cfg.display, app->nodes, app->weather, app->time);

    if (!app->configured) {
        // ------------------- Primo avvio: modalità AP -------------------
        Logger::warn(kTag, "Nessuna configurazione: avvio in modalità AP");
        app->wifi.beginAp();
        app->web.begin();
        app->display.showSetupScreen(app->wifi.apSsid(), app->wifi.ip());
        app->web.onReboot(scheduleReboot);
        return;
    }
    // ---------------------- Avvio normale ----------------------
    wireModules();

    app->wifi.beginSta(cfg.wifi, cfg.gateway.name);
    if (!app->lora.begin(cfg.lora)) {
        Logger::error(kTag, "LoRa non disponibile: proseguo senza radio");
    }
    app->mqtt.setCaCert(leCaCrt);
    app->mqtt.begin(cfg.mqtt, cfg.gateway.name);
    app->weather.begin(cfg.weatherApi);
    app->ota.begin(cfg.nodeOta, app->lora, app->nodes);
    app->web.begin();
    app->display.requestRefresh();

    Logger::info(kTag, "Avvio completato");
}

void loop() {
    app->wifi.loop();
    app->web.loop();

    if (app->configured) {
        const bool netUp = app->wifi.isConnected();
        if (!app->timeSyncLogged && app->time.isSynced()) {
            app->timeSyncLogged = true;
            Logger::info(kTag, "Ora sincronizzata: %s %s",
                         app->time.dateitalian().c_str(),
                         app->time.timeShort().c_str());
            app->display.markDirty();
        }
        app->lora.loop();
        // Con verifica del certificato TLS la connessione può partire solo
        // a orologio sincronizzato: mbedTLS controlla le date di validità
        // del certificato (a ora non valida l'handshake fallirebbe sempre).
        const bool mqttUp =
            netUp && (!app->config.get().mqtt.useTls() ||
                      !app->mqtt.usesCaCert() || app->time.isSynced());
        app->mqtt.loop(mqttUp);
        app->weather.loop(netUp);
        app->ota.loop(netUp);
        app->display.loop();
        if (netUp) {
            ArduinoOTA.handle();
        }

        // Controllo nodi offline e pubblicazione stato ogni pochi secondi.
        const uint32_t now = millis();
        if (now - app->lastNodeLoopMs > 5000) {
            app->lastNodeLoopMs = now;
            app->nodes.loop();
        }
        if (now - app->lastInfoPubMs > 60000) {
            app->lastInfoPubMs = now;
            publishGatewayInfo();
        }
    }

    // Riavvio ritardato (per completare le risposte HTTP/MQTT in corso).
    if (app->rebootAtMs != 0 && millis() > app->rebootAtMs) {
        Logger::info(kTag, "Riavvio...");
        delay(100);
        ESP.restart();
    }
}
