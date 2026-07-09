/**
 * @file LoraManager.h
 * @brief Ricetrasmissione LoRa (SX1276) non bloccante basata su RadioLib.
 *
 * La ricezione è guidata da interrupt (DIO0): il loop principale si limita a
 * controllare un flag. Anche la trasmissione è asincrona: send() avvia il
 * TX e la macchina a stati torna in ricezione al termine.
 */
#pragma once

#include <Arduino.h>
#include <RadioLib.h>

#include <functional>

#include "ConfigManager.h"

/** @brief Stato radio. */
enum class LoraState : uint8_t {
    Off,          ///< Non inizializzata (errore hardware o begin non chiamato)
    Receiving,    ///< In ascolto.
    Transmitting  ///< Trasmissione in corso.
};

/**
 * @brief Driver di alto livello per il modulo LoRa del gateway.
 */
class LoraManager {
   public:
    /**
     * @brief Callback pacchetto ricevuto.
     * @param data Payload (terminato da \0 per comodità).
     * @param len  Lunghezza payload.
     * @param rssi RSSI in dBm.
     * @param snr  SNR in dB.
     */
    using PacketFn = std::function<void(const uint8_t* data, size_t len,
                                        float rssi, float snr)>;

    /**
     * @brief Inizializza modulo e parametri radio.
     * @return true se il modulo risponde ed è in ascolto.
     */
    bool begin(const LoraConfig& cfg);

    /** @brief Da chiamare a ogni giro di loop(). */
    void loop();

    /**
     * @brief Trasmette un pacchetto (asincrono).
     * @return false se la radio è occupata o spenta.
     */
    bool send(const uint8_t* data, size_t len);

    /** @brief Comodità: trasmette una stringa. */
    bool send(const String& s) {
        return send(reinterpret_cast<const uint8_t*>(s.c_str()), s.length());
    }

    void onPacket(PacketFn cb) { onPacket_ = cb; }

    bool isReady() const { return state_ != LoraState::Off; }
    bool isBusy() const { return state_ == LoraState::Transmitting; }
    LoraState state() const { return state_; }

    float lastRssi() const { return lastRssi_; }
    float lastSnr() const { return lastSnr_; }
    uint32_t packetsReceived() const { return packetsReceived_; }
    /** @brief millis() dell'ultimo pacchetto ricevuto (0 = mai). */
    uint32_t lastPacketMs() const { return lastPacketMs_; }

   private:
    static void IRAM_ATTR onDio0Isr();

    void startReceive();

    SPIClass* spi_ = nullptr;
    SX1276* radio_ = nullptr;
    Module* module_ = nullptr;

    LoraState state_ = LoraState::Off;
    PacketFn onPacket_;
    float lastRssi_ = 0;
    float lastSnr_ = 0;
    uint32_t packetsReceived_ = 0;
    uint32_t lastPacketMs_ = 0;
    uint32_t txStartedMs_ = 0;

    static volatile bool irqFlag_;
};
