/**
 * @file DisplayManager.h
 * @brief Rendering della dashboard sul display e-paper 4.7" (960x540).
 *
 * Layout (ispirato a DashboardEsempio.png):
 *   - header: stazione corrente + batteria, data/ora, icone WiFi/MQTT/LoRa
 *   - barra eventi (solo quando ci sono anomalie)
 *   - colonna sinistra: bussola del vento, alba/tramonto, fase lunare
 *   - centro: temperatura/umidità correnti, condizioni OpenWeatherMap
 *   - destra: icona meteo grande
 *   - striscia previsioni a 3 ore (7 slot)
 *   - 4 grafici storici a 48h (pressione, temperatura, umidità, pioggia)
 *
 * Con più stazioni collegate i dati vengono mostrati a rotazione, una
 * stazione alla volta. Il refresh avviene solo quando necessario.
 */
#pragma once

#include <Arduino.h>

#include <functional>

#include "ConfigManager.h"
#include "Gfx/Gfx.h"
#include "NodeManager.h"
#include "TimeManager.h"
#include "WeatherManager.h"

/** @brief Stato del gateway mostrato su header e barra eventi. */
struct GatewayStatus {
    bool wifiConnected = false;
    bool apMode = false;
    String ip;
    bool mqttConnected = false;
    bool mqttConfigured = false;
    bool loraReady = false;
    bool otaInProgress = false;
    int otaProgress = 0;
    String otaNode;
    bool weatherFailed = false;
};

/**
 * @brief Gestisce framebuffer, layout e politica di refresh del display.
 */
class DisplayManager {
   public:
    using StatusFn = std::function<GatewayStatus()>;

    /**
     * @brief Inizializza il pannello e alloca il framebuffer in PSRAM.
     */
    bool begin(const DisplayConfig& cfg, NodeManager& nodes,
               WeatherManager& weather, TimeManager& time);

    /** @brief Fornisce lo stato del gateway a ogni render. */
    void setStatusProvider(StatusFn fn) { status_ = fn; }

    /**
     * @brief Politica di refresh: ridisegna al cambio di minuto, alla
     *        rotazione della stazione o quando i dati sono cambiati.
     */
    void loop();

    /** @brief Segnala che i dati sono cambiati (refresh al prossimo loop). */
    void markDirty() { dirty_ = true; }

    /** @brief Forza un refresh immediato (comando MQTT/Web). */
    void requestRefresh();

    /**
     * @brief Schermata di prima configurazione (modalità Access Point).
     */
    void showSetupScreen(const String& apSsid, const String& ip);

   private:
    static constexpr int kW = 960;
    static constexpr int kH = 540;
    static constexpr uint32_t kMinRefreshMs = 10000;

    // Confini del layout
    static constexpr int kHeaderH = 46;
    static constexpr int kEventH = 28;
    static constexpr int kMainTop = kHeaderH + kEventH;   // 74
    static constexpr int kForecastTop = 306;
    static constexpr int kGraphTop = 412;
    static constexpr int kLeftColW = 232;

    void render();
    void flush();

    void drawHeader(const GatewayStatus& st, const NodeInfo* node);
    void drawEvents(const GatewayStatus& st);
    void drawCompass(const NodeInfo* node);
    void drawSunMoon();
    void drawCurrent(const NodeInfo* node);
    void drawForecast();
    void drawGraphs(const NodeInfo* node);
    void drawChart(int x, int y, int w, int h, const char* title,
                   const float* data, size_t n, bool bars);

    /** @brief Nome cardinale italiano da gradi (es. 270 -> "O"). */
    static const char* cardinal(int deg);

    DisplayConfig cfg_;
    NodeManager* nodes_ = nullptr;
    WeatherManager* weather_ = nullptr;
    TimeManager* time_ = nullptr;
    StatusFn status_;

    uint8_t* fb_ = nullptr;
    Gfx* gfx_ = nullptr;

    bool ready_ = false;
    bool dirty_ = true;
    bool forceNow_ = false;
    int lastMinute_ = -1;
    uint32_t lastRefreshMs_ = 0;
    uint32_t lastRotationMs_ = 0;
    size_t rotationIndex_ = 0;
    uint32_t refreshCount_ = 0;
};
