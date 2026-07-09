/**
 * @file WeatherManager.cpp
 * @brief Implementazione dell'integrazione OpenWeatherMap.
 */
#include "WeatherManager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Logger.h>
#include <WiFiClientSecure.h>

namespace {
constexpr const char* kTag = "OWM";
constexpr const char* kHost = "api.openweathermap.org";
constexpr uint32_t kTaskStack = 12288;
constexpr uint32_t kHttpTimeoutMs = 10000;
constexpr uint32_t kRetryOnErrorS = 120;
}  // namespace

void WeatherManager::begin(const WeatherApiConfig& cfg) { cfg_ = cfg; }

void WeatherManager::loop(bool networkUp) {
    // Consegna dei risultati prodotti dal task.
    if (resultReady_.load()) {
        resultReady_.store(false);
        if (pendingOk_) {
            current_ = pendingCurrent_;
            memcpy(forecast_, pendingForecast_, sizeof(forecast_));
            lastFailed_ = false;
            Logger::info(kTag, "Dati meteo aggiornati: %.1f\xC2\xB0 %s",
                         current_.temp, current_.description);
            if (onUpdated_) {
                onUpdated_();
            }
        } else {
            lastFailed_ = true;
            Logger::warn(kTag, "Aggiornamento meteo fallito");
        }
    }

    if (!cfg_.enabled || cfg_.apiKey.isEmpty() || !networkUp ||
        taskRunning_.load()) {
        return;
    }

    const uint32_t intervalS =
        lastFailed_ && cfg_.updateInterval > kRetryOnErrorS ? kRetryOnErrorS
                                                            : cfg_.updateInterval;
    const bool due =
        !everFetched_ || millis() - lastFetchMs_ >= intervalS * 1000UL;
    if (!due && !forceUpdate_) {
        return;
    }
    forceUpdate_ = false;
    everFetched_ = true;
    lastFetchMs_ = millis();

    taskRunning_.store(true);
    const BaseType_t ok = xTaskCreatePinnedToCore(
        taskEntry, "owm_fetch", kTaskStack, this, 1, nullptr, 0);
    if (ok != pdPASS) {
        taskRunning_.store(false);
        Logger::error(kTag, "Impossibile creare il task di fetch");
    }
}

void WeatherManager::taskEntry(void* self) {
    auto* mgr = static_cast<WeatherManager*>(self);
    mgr->runFetch();
    mgr->taskRunning_.store(false);
    vTaskDelete(nullptr);
}

void WeatherManager::runFetch() {
    pendingOk_ = fetchCurrent() && fetchForecast();
    resultReady_.store(true);
}

bool WeatherManager::fetchCurrent() {
    WiFiClientSecure client;
    client.setInsecure();  // niente validazione CA: solo dati meteo pubblici
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);

    // Richiesta generata dinamicamente come da specifica.
    String addr = String("https://") + kHost + "/data/2.5/weather?q=" +
                  cfg_.city + "," + cfg_.country +
                  "&units=metric&lang=it&appid=" + cfg_.apiKey;
    if (!http.begin(client, addr)) {
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Logger::warn(kTag, "HTTP %d da /weather", code);
        http.end();
        return false;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) {
        Logger::warn(kTag, "JSON /weather non valido: %s", err.c_str());
        return false;
    }

    CurrentWeather& w = pendingCurrent_;
    w = CurrentWeather{};
    w.temp = doc["main"]["temp"] | NAN;
    w.feelsLike = doc["main"]["feels_like"] | NAN;
    w.tempMin = doc["main"]["temp_min"] | NAN;
    w.tempMax = doc["main"]["temp_max"] | NAN;
    w.humidity = doc["main"]["humidity"] | 0;
    w.pressure = doc["main"]["pressure"] | 0;
    w.windSpeed = doc["wind"]["speed"] | NAN;
    w.windDeg = doc["wind"]["deg"] | -1;
    w.visibility = doc["visibility"] | 0;
    w.clouds = doc["clouds"]["all"] | 0;
    strlcpy(w.icon, doc["weather"][0]["icon"] | "", sizeof(w.icon));
    strlcpy(w.description, doc["weather"][0]["description"] | "",
            sizeof(w.description));
    w.sunrise = doc["sys"]["sunrise"] | 0;
    w.sunset = doc["sys"]["sunset"] | 0;
    w.fetchedAt = time(nullptr);
    w.valid = !isnan(w.temp);
    return w.valid;
}

bool WeatherManager::fetchForecast() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(kHttpTimeoutMs);

    String addr = String("https://") + kHost + "/data/2.5/forecast?q=" +
                  cfg_.city + "," + cfg_.country + "&cnt=" +
                  String(kForecastSlots) +
                  "&units=metric&lang=it&appid=" + cfg_.apiKey;
    if (!http.begin(client, addr)) {
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Logger::warn(kTag, "HTTP %d da /forecast", code);
        http.end();
        return false;
    }

    // Filtro: dalla risposta servono solo pochi campi per slot.
    JsonDocument filter;
    filter["list"][0]["dt"] = true;
    filter["list"][0]["main"]["temp_min"] = true;
    filter["list"][0]["main"]["temp_max"] = true;
    filter["list"][0]["weather"][0]["icon"] = true;
    filter["list"][0]["pop"] = true;

    JsonDocument doc;
    const DeserializationError err = deserializeJson(
        doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err) {
        Logger::warn(kTag, "JSON /forecast non valido: %s", err.c_str());
        return false;
    }

    JsonArrayConst list = doc["list"];
    size_t i = 0;
    for (JsonObjectConst item : list) {
        if (i >= kForecastSlots) {
            break;
        }
        ForecastSlot& slot = pendingForecast_[i];
        slot = ForecastSlot{};
        slot.dt = item["dt"] | 0;
        slot.tempMin = item["main"]["temp_min"] | NAN;
        slot.tempMax = item["main"]["temp_max"] | NAN;
        strlcpy(slot.icon, item["weather"][0]["icon"] | "",
                sizeof(slot.icon));
        slot.pop = item["pop"] | 0.0f;
        ++i;
    }
    return i > 0;
}
