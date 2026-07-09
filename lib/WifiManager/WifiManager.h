/**
 * @file WifiManager.h
 * @brief Gestione Wi-Fi: station con riconnessione automatica o Access Point
 *        di configurazione con captive portal.
 */
#pragma once

#include <Arduino.h>
#include <DNSServer.h>

#include "ConfigManager.h"

/** @brief Stato della macchina a stati Wi-Fi. */
enum class WifiState : uint8_t {
    Idle,        ///< Non inizializzato.
    Connecting,  ///< Tentativo di connessione in corso.
    Connected,   ///< Connesso alla rete configurata.
    WaitRetry,   ///< In attesa prima di un nuovo tentativo.
    ApMode,      ///< Access Point di configurazione attivo.
};

/**
 * @brief Macchina a stati non bloccante per la connettività Wi-Fi.
 */
class WifiManager {
   public:
    /**
     * @brief Avvia in modalità station con le credenziali configurate.
     */
    void beginSta(const WifiConfig& cfg, const String& hostname);

    /**
     * @brief Avvia l'Access Point di prima configurazione
     *        (SSID "MeteoGateway-XXXX" + DNS captive portal).
     */
    void beginAp();

    /** @brief Da chiamare a ogni giro di loop(). */
    void loop();

    bool isConnected() const { return state_ == WifiState::Connected; }
    bool isApMode() const { return state_ == WifiState::ApMode; }
    WifiState state() const { return state_; }

    /** @brief IP corrente (station o AP). */
    String ip() const;

    /** @brief RSSI in dBm (0 se non connesso). */
    int rssi() const;

    /** @brief SSID dell'Access Point (valido in modalità AP). */
    String apSsid() const { return apSsid_; }

    /** @brief Callback invocata alla prima connessione riuscita. */
    void onConnected(std::function<void()> cb) { onConnected_ = cb; }

   private:
    static constexpr uint32_t kConnectTimeoutMs = 20000;
    static constexpr uint32_t kRetryIntervalMs = 15000;

    WifiState state_ = WifiState::Idle;
    WifiConfig cfg_;
    String hostname_;
    String apSsid_;
    uint32_t stateSince_ = 0;
    bool wasConnected_ = false;
    DNSServer dns_;
    std::function<void()> onConnected_;
};
