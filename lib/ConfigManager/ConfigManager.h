/**
 * @file ConfigManager.h
 * @brief Gestione della configurazione persistente su LittleFS (JSON).
 *
 * Nessun parametro è hardcoded nel firmware: tutto ciò che è regolabile
 * vive nel file /config.json. Se il file non esiste il gateway parte in
 * modalità Access Point per la prima configurazione.
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/** @brief Configurazione Wi-Fi (modalità station). */
struct WifiConfig {
    String ssid;
    String password;
};

/** @brief Configurazione broker MQTT. */
struct MqttConfig {
    String host;
    uint16_t port = 1883;
    String username;
    String password;
    String baseTopic = "meteo";
    bool tls = false;      ///< true = MQTT su TLS (tipicamente porta 8883)
    String clientId;       ///< vuoto = generato dal nome gateway + MAC

    /** @brief true se la connessione va cifrata (flag esplicito o porta 8883). */
    bool useTls() const { return tls || port == 8883; }
};

/**
 * @brief Pin del modulo LoRa SX1276 (regolabili da Web UI).
 *
 * Default per LILYGO T5 4.7" S3: bus SPI dello slot microSD
 * (SCK 11, MOSI 15, MISO 16, CS 42) e connettore I2C per RST/DIO0
 * (17/18). Il pannello e-paper occupa GPIO 0-8, 12, 13, 38, 40, 41:
 * non usarli. Adattare i pin al cablaggio reale dalla Web UI.
 */
struct LoraPins {
    int8_t sck = 11;
    int8_t miso = 16;
    int8_t mosi = 15;
    int8_t cs = 42;
    int8_t rst = 17;
    int8_t dio0 = 18;
    int8_t dio1 = -1;
};

/** @brief Parametri radio LoRa. */
struct LoraConfig {
    uint32_t frequency = 868000000;  ///< Hz
    uint32_t bandwidth = 125000;     ///< Hz
    uint8_t spreadingFactor = 9;
    uint8_t codingRate = 5;          ///< Denominatore (4/x)
    int8_t txPower = 20;             ///< dBm
    uint8_t syncWord = 0x12;
    LoraPins pins;
};

/** @brief Identità del gateway. */
struct GatewayConfig {
    String name = "Gateway01";
    String timezone = "Europe/Rome";
};

/** @brief Integrazione OpenWeatherMap. */
struct WeatherApiConfig {
    bool enabled = true;
    String city = "Padova";
    String country = "IT";
    String apiKey;
    uint32_t updateInterval = 1800;  ///< secondi
};

/** @brief OTA dei nodi remoti da release GitHub. */
struct NodeOtaConfig {
    bool enabled = true;
    String repo = "CarloFalco/WeatherStation";  ///< owner/name
    String assetName = "firmware.bin";
    uint32_t checkInterval = 21600;  ///< secondi
};

/** @brief Comportamento del display. */
struct DisplayConfig {
    uint32_t rotationInterval = 30;   ///< s tra una stazione e la successiva
    uint32_t fullRefreshEvery = 10;   ///< refresh completi (anti ghosting)
    uint32_t nodeTimeout = 900;       ///< s senza pacchetti = nodo offline
};

/** @brief Configurazione logging. */
struct LogConfig {
    String level = "info";
    bool mqtt = false;
    bool file = false;
};

/** @brief Configurazione completa del gateway. */
struct Config {
    WifiConfig wifi;
    MqttConfig mqtt;
    LoraConfig lora;
    GatewayConfig gateway;
    WeatherApiConfig weatherApi;
    NodeOtaConfig nodeOta;
    DisplayConfig display;
    LogConfig log;
};

/**
 * @brief Carica/salva la configurazione da/verso LittleFS.
 */
class ConfigManager {
   public:
    /**
     * @brief Monta LittleFS (formattandolo al primo avvio se necessario).
     * @return true se il filesystem è utilizzabile.
     */
    bool begin();

    /** @brief true se /config.json esiste. */
    bool exists() const;

    /**
     * @brief Carica la configurazione dal file.
     * @return true se il file esiste ed è un JSON valido.
     */
    bool load();

    /** @brief Salva la configurazione corrente su file. */
    bool save() const;

    /**
     * @brief Ripristino di fabbrica: cancella tutti i dati salvati su
     *        LittleFS (configurazione, log, firmware nodi in cache) e riporta
     *        la configurazione in RAM ai valori di default.
     *
     * Dopo la chiamata il gateway va riavviato: non trovando la
     * configurazione ripartirà in modalità Access Point.
     * @return true se la cancellazione è riuscita.
     */
    bool factoryReset();

    /** @brief Serializza la configurazione in un documento JSON. */
    void toJson(JsonDocument& doc, bool includeSecrets = true) const;

    /**
     * @brief Applica un documento JSON alla configurazione.
     *
     * I campi assenti mantengono il valore corrente; i campi password/apikey
     * vuoti non sovrascrivono quelli salvati (per permettere update parziali
     * dalla Web UI senza riesporre i segreti).
     */
    void fromJson(const JsonDocument& doc);

    Config& get() { return cfg_; }
    const Config& get() const { return cfg_; }

   private:
    Config cfg_;
};
