/**
 * @file ConfigManager.cpp
 * @brief Implementazione della gestione configurazione.
 */
#include "ConfigManager.h"

#include <LittleFS.h>
#include <Logger.h>

namespace {
constexpr const char* kTag = "Config";
constexpr const char* kConfigFile = "/config.json";

/** @brief Copia una stringa dal JSON solo se presente e non vuota. */
void assignIfPresent(JsonVariantConst v, String& out,
                     bool keepIfEmpty = false) {
    if (v.isNull()) {
        return;
    }
    const char* s = v.as<const char*>();
    if (s == nullptr) {
        return;
    }
    if (keepIfEmpty && s[0] == '\0') {
        return;  // non sovrascrivere un segreto salvato con una stringa vuota
    }
    out = s;
}

template <typename T>
void assignNum(JsonVariantConst v, T& out) {
    if (!v.isNull()) {
        out = v.as<T>();
    }
}

void assignBool(JsonVariantConst v, bool& out) {
    if (!v.isNull()) {
        out = v.as<bool>();
    }
}
}  // namespace

bool ConfigManager::begin() {
    if (!LittleFS.begin(true)) {
        Logger::error(kTag, "Impossibile montare LittleFS");
        return false;
    }
    return true;
}

bool ConfigManager::exists() const { return LittleFS.exists(kConfigFile); }

bool ConfigManager::load() {
    File f = LittleFS.open(kConfigFile, "r");
    if (!f) {
        Logger::warn(kTag, "File di configurazione assente");
        return false;
    }
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Logger::error(kTag, "JSON di configurazione non valido: %s",
                      err.c_str());
        return false;
    }
    fromJson(doc);
    Logger::info(kTag, "Configurazione caricata (gateway: %s)",
                 cfg_.gateway.name.c_str());
    return true;
}

bool ConfigManager::save() const {
    File f = LittleFS.open(kConfigFile, "w");
    if (!f) {
        Logger::error(kTag, "Impossibile aprire %s in scrittura", kConfigFile);
        return false;
    }
    JsonDocument doc;
    toJson(doc, true);
    const size_t written = serializeJson(doc, f);
    f.close();
    if (written == 0) {
        Logger::error(kTag, "Scrittura configurazione fallita");
        return false;
    }
    Logger::info(kTag, "Configurazione salvata (%u byte)",
                 static_cast<unsigned>(written));
    return true;
}

bool ConfigManager::factoryReset() {
    Logger::warn(kTag, "Ripristino di fabbrica: cancellazione dati salvati");
    // File scritti dal firmware durante il normale funzionamento.
    static const char* kFiles[] = {kConfigFile, "/log.txt", "/log.old.txt",
                                    "/node_fw.bin"};
    bool ok = true;
    for (const char* path : kFiles) {
        if (LittleFS.exists(path) && !LittleFS.remove(path)) {
            Logger::error(kTag, "Impossibile eliminare %s", path);
            ok = false;
        }
    }
    // Riporta la configurazione in RAM ai valori di default.
    cfg_ = Config{};
    return ok;
}

void ConfigManager::toJson(JsonDocument& doc, bool includeSecrets) const {
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = cfg_.wifi.ssid;
    wifi["password"] = includeSecrets ? cfg_.wifi.password : String("");

    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["host"] = cfg_.mqtt.host;
    mqtt["port"] = cfg_.mqtt.port;
    mqtt["username"] = cfg_.mqtt.username;
    mqtt["password"] = includeSecrets ? cfg_.mqtt.password : String("");
    mqtt["base_topic"] = cfg_.mqtt.baseTopic;
    mqtt["tls"] = cfg_.mqtt.tls;
    mqtt["client_id"] = cfg_.mqtt.clientId;

    JsonObject lora = doc["lora"].to<JsonObject>();
    lora["frequency"] = cfg_.lora.frequency;
    lora["bandwidth"] = cfg_.lora.bandwidth;
    lora["spreading_factor"] = cfg_.lora.spreadingFactor;
    lora["coding_rate"] = cfg_.lora.codingRate;
    lora["tx_power"] = cfg_.lora.txPower;
    lora["sync_word"] = cfg_.lora.syncWord;
    JsonObject pins = lora["pins"].to<JsonObject>();
    pins["sck"] = cfg_.lora.pins.sck;
    pins["miso"] = cfg_.lora.pins.miso;
    pins["mosi"] = cfg_.lora.pins.mosi;
    pins["cs"] = cfg_.lora.pins.cs;
    pins["rst"] = cfg_.lora.pins.rst;
    pins["dio0"] = cfg_.lora.pins.dio0;
    pins["dio1"] = cfg_.lora.pins.dio1;

    JsonObject gw = doc["gateway"].to<JsonObject>();
    gw["name"] = cfg_.gateway.name;
    gw["timezone"] = cfg_.gateway.timezone;

    JsonObject wapi = doc["weather_api"].to<JsonObject>();
    wapi["enabled"] = cfg_.weatherApi.enabled;
    wapi["city"] = cfg_.weatherApi.city;
    wapi["country"] = cfg_.weatherApi.country;
    wapi["api_key"] = includeSecrets ? cfg_.weatherApi.apiKey : String("");
    wapi["update_interval"] = cfg_.weatherApi.updateInterval;

    JsonObject ota = doc["node_ota"].to<JsonObject>();
    ota["enabled"] = cfg_.nodeOta.enabled;
    ota["repo"] = cfg_.nodeOta.repo;
    ota["asset_name"] = cfg_.nodeOta.assetName;
    ota["check_interval"] = cfg_.nodeOta.checkInterval;

    JsonObject disp = doc["display"].to<JsonObject>();
    disp["rotation_interval"] = cfg_.display.rotationInterval;
    disp["full_refresh_every"] = cfg_.display.fullRefreshEvery;
    disp["node_timeout"] = cfg_.display.nodeTimeout;

    JsonObject log = doc["log"].to<JsonObject>();
    log["level"] = cfg_.log.level;
    log["mqtt"] = cfg_.log.mqtt;
    log["file"] = cfg_.log.file;
}

void ConfigManager::fromJson(const JsonDocument& doc) {
    JsonObjectConst wifi = doc["wifi"];
    assignIfPresent(wifi["ssid"], cfg_.wifi.ssid);
    assignIfPresent(wifi["password"], cfg_.wifi.password, true);

    JsonObjectConst mqtt = doc["mqtt"];
    assignIfPresent(mqtt["host"], cfg_.mqtt.host);
    assignNum(mqtt["port"], cfg_.mqtt.port);
    assignIfPresent(mqtt["username"], cfg_.mqtt.username);
    assignIfPresent(mqtt["password"], cfg_.mqtt.password, true);
    assignIfPresent(mqtt["base_topic"], cfg_.mqtt.baseTopic);
    assignBool(mqtt["tls"], cfg_.mqtt.tls);
    assignIfPresent(mqtt["client_id"], cfg_.mqtt.clientId);

    JsonObjectConst lora = doc["lora"];
    assignNum(lora["frequency"], cfg_.lora.frequency);
    assignNum(lora["bandwidth"], cfg_.lora.bandwidth);
    assignNum(lora["spreading_factor"], cfg_.lora.spreadingFactor);
    assignNum(lora["coding_rate"], cfg_.lora.codingRate);
    assignNum(lora["tx_power"], cfg_.lora.txPower);
    assignNum(lora["sync_word"], cfg_.lora.syncWord);
    JsonObjectConst pins = lora["pins"];
    assignNum(pins["sck"], cfg_.lora.pins.sck);
    assignNum(pins["miso"], cfg_.lora.pins.miso);
    assignNum(pins["mosi"], cfg_.lora.pins.mosi);
    assignNum(pins["cs"], cfg_.lora.pins.cs);
    assignNum(pins["rst"], cfg_.lora.pins.rst);
    assignNum(pins["dio0"], cfg_.lora.pins.dio0);
    assignNum(pins["dio1"], cfg_.lora.pins.dio1);

    JsonObjectConst gw = doc["gateway"];
    assignIfPresent(gw["name"], cfg_.gateway.name);
    assignIfPresent(gw["timezone"], cfg_.gateway.timezone);

    JsonObjectConst wapi = doc["weather_api"];
    assignBool(wapi["enabled"], cfg_.weatherApi.enabled);
    assignIfPresent(wapi["city"], cfg_.weatherApi.city);
    assignIfPresent(wapi["country"], cfg_.weatherApi.country);
    assignIfPresent(wapi["api_key"], cfg_.weatherApi.apiKey, true);
    assignNum(wapi["update_interval"], cfg_.weatherApi.updateInterval);

    JsonObjectConst ota = doc["node_ota"];
    assignBool(ota["enabled"], cfg_.nodeOta.enabled);
    assignIfPresent(ota["repo"], cfg_.nodeOta.repo);
    assignIfPresent(ota["asset_name"], cfg_.nodeOta.assetName);
    assignNum(ota["check_interval"], cfg_.nodeOta.checkInterval);

    JsonObjectConst disp = doc["display"];
    assignNum(disp["rotation_interval"], cfg_.display.rotationInterval);
    assignNum(disp["full_refresh_every"], cfg_.display.fullRefreshEvery);
    assignNum(disp["node_timeout"], cfg_.display.nodeTimeout);

    JsonObjectConst log = doc["log"];
    assignIfPresent(log["level"], cfg_.log.level);
    assignBool(log["mqtt"], cfg_.log.mqtt);
    assignBool(log["file"], cfg_.log.file);
}
