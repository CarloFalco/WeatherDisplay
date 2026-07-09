/**
 * @file WifiManager.cpp
 * @brief Implementazione della macchina a stati Wi-Fi.
 */
#include "WifiManager.h"

#include <Logger.h>
#include <WiFi.h>

namespace {
constexpr const char* kTag = "WiFi";
}

void WifiManager::beginSta(const WifiConfig& cfg, const String& hostname) {
    cfg_ = cfg;
    hostname_ = hostname;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname_.c_str());
    WiFi.setAutoReconnect(false);  // la riconnessione la gestiamo noi
    WiFi.begin(cfg_.ssid.c_str(), cfg_.password.c_str());
    state_ = WifiState::Connecting;
    stateSince_ = millis();
    Logger::info(kTag, "Connessione a \"%s\"...", cfg_.ssid.c_str());
}

void WifiManager::beginAp() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "MeteoGateway-%02X%02X", mac[4], mac[5]);
    apSsid_ = ssid;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid_.c_str());
    dns_.start(53, "*", WiFi.softAPIP());
    state_ = WifiState::ApMode;
    stateSince_ = millis();
    Logger::info(kTag, "Access Point \"%s\" attivo su %s", ssid,
                 WiFi.softAPIP().toString().c_str());
}

void WifiManager::loop() {
    switch (state_) {
        case WifiState::Idle:
            break;

        case WifiState::Connecting:
            if (WiFi.status() == WL_CONNECTED) {
                state_ = WifiState::Connected;
                stateSince_ = millis();
                Logger::info(kTag, "Connesso, IP %s (RSSI %d dBm)",
                             WiFi.localIP().toString().c_str(), WiFi.RSSI());
                if (!wasConnected_ && onConnected_) {
                    onConnected_();
                }
                wasConnected_ = true;
            } else if (millis() - stateSince_ > kConnectTimeoutMs) {
                Logger::warn(kTag, "Timeout di connessione, riprovo tra %lus",
                             static_cast<unsigned long>(kRetryIntervalMs /
                                                        1000));
                WiFi.disconnect();
                state_ = WifiState::WaitRetry;
                stateSince_ = millis();
            }
            break;

        case WifiState::Connected:
            if (WiFi.status() != WL_CONNECTED) {
                Logger::warn(kTag, "Connessione persa, riconnessione...");
                WiFi.disconnect();
                state_ = WifiState::WaitRetry;
                stateSince_ = millis();
            }
            break;

        case WifiState::WaitRetry:
            if (millis() - stateSince_ > kRetryIntervalMs) {
                WiFi.begin(cfg_.ssid.c_str(), cfg_.password.c_str());
                state_ = WifiState::Connecting;
                stateSince_ = millis();
                Logger::info(kTag, "Nuovo tentativo di connessione...");
            }
            break;

        case WifiState::ApMode:
            dns_.processNextRequest();
            break;
    }
}

String WifiManager::ip() const {
    if (state_ == WifiState::ApMode) {
        return WiFi.softAPIP().toString();
    }
    if (state_ == WifiState::Connected) {
        return WiFi.localIP().toString();
    }
    return "-";
}

int WifiManager::rssi() const {
    return state_ == WifiState::Connected ? WiFi.RSSI() : 0;
}
