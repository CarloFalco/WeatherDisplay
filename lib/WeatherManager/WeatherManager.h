/**
 * @file WeatherManager.h
 * @brief Integrazione OpenWeatherMap (condizioni attuali + previsioni).
 *
 * Le richieste HTTPS avvengono in un task FreeRTOS dedicato per non
 * bloccare il loop principale; i risultati vengono consegnati al thread
 * principale tramite un flag atomico.
 */
#pragma once

#include <Arduino.h>

#include <atomic>
#include <ctime>
#include <functional>

#include "ConfigManager.h"

/** @brief Numero di slot di previsione mostrati (3 ore ciascuno). */
constexpr size_t kForecastSlots = 7;

/** @brief Condizioni meteo attuali da OpenWeatherMap. */
struct CurrentWeather {
    bool valid = false;
    float temp = NAN;       ///< °C
    float feelsLike = NAN;  ///< °C percepita
    float tempMin = NAN;
    float tempMax = NAN;
    int humidity = 0;       ///< %
    int pressure = 0;       ///< hPa
    float windSpeed = NAN;  ///< m/s
    int windDeg = -1;
    int visibility = 0;     ///< m
    int clouds = 0;         ///< %
    char icon[4] = "";      ///< codice icona OWM ("10d")
    char description[48] = "";
    time_t sunrise = 0;
    time_t sunset = 0;
    time_t fetchedAt = 0;
};

/** @brief Singolo slot di previsione (passo 3 ore). */
struct ForecastSlot {
    time_t dt = 0;
    float tempMin = NAN;
    float tempMax = NAN;
    char icon[4] = "";
    float pop = 0;  ///< probabilità precipitazioni 0..1
};

/**
 * @brief Scarica e mantiene i dati OpenWeatherMap.
 */
class WeatherManager {
   public:
    using UpdatedFn = std::function<void()>;

    void begin(const WeatherApiConfig& cfg);

    /**
     * @brief Pianifica gli aggiornamenti periodici e consegna i risultati.
     * @param networkUp true se il Wi-Fi è connesso.
     */
    void loop(bool networkUp);

    /** @brief Forza un aggiornamento al prossimo giro di loop. */
    void forceUpdate() { forceUpdate_ = true; }

    const CurrentWeather& current() const { return current_; }
    const ForecastSlot* forecast() const { return forecast_; }
    bool isUpdating() const { return taskRunning_.load(); }
    /** @brief true se l'ultimo aggiornamento è fallito. */
    bool lastFailed() const { return lastFailed_; }

    /** @brief Callback (thread principale) quando arrivano dati nuovi. */
    void onUpdated(UpdatedFn cb) { onUpdated_ = cb; }

   private:
    static void taskEntry(void* self);
    void runFetch();
    bool fetchCurrent();
    bool fetchForecast();

    WeatherApiConfig cfg_;
    CurrentWeather current_;
    ForecastSlot forecast_[kForecastSlots];

    // Buffer scritti dal task; copiati nel thread principale a flag alzato.
    CurrentWeather pendingCurrent_;
    ForecastSlot pendingForecast_[kForecastSlots];
    std::atomic<bool> resultReady_{false};
    std::atomic<bool> taskRunning_{false};
    bool pendingOk_ = false;
    bool lastFailed_ = false;
    bool forceUpdate_ = false;
    uint32_t lastFetchMs_ = 0;
    bool everFetched_ = false;
    UpdatedFn onUpdated_;
};
