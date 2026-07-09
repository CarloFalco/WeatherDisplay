/**
 * @file LoraManager.cpp
 * @brief Implementazione del driver LoRa.
 */
#include "LoraManager.h"

#include <Logger.h>

namespace {
constexpr const char* kTag = "LoRa";
constexpr size_t kMaxPacket = 256;
constexpr uint32_t kTxTimeoutMs = 5000;
}  // namespace

volatile bool LoraManager::irqFlag_ = false;

void IRAM_ATTR LoraManager::onDio0Isr() { irqFlag_ = true; }

bool LoraManager::begin(const LoraConfig& cfg) {
    spi_ = new SPIClass(HSPI);
    spi_->begin(cfg.pins.sck, cfg.pins.miso, cfg.pins.mosi, cfg.pins.cs);

    module_ = new Module(cfg.pins.cs, cfg.pins.dio0, cfg.pins.rst,
                         cfg.pins.dio1, *spi_);
    radio_ = new SX1276(module_);

    const float freqMhz = cfg.frequency / 1e6f;
    const float bwKhz = cfg.bandwidth / 1e3f;
    const int16_t rc =
        radio_->begin(freqMhz, bwKhz, cfg.spreadingFactor, cfg.codingRate,
                      cfg.syncWord, cfg.txPower);
    if (rc != RADIOLIB_ERR_NONE) {
        Logger::error(kTag, "Inizializzazione SX1276 fallita (codice %d)", rc);
        state_ = LoraState::Off;
        return false;
    }
    radio_->setCRC(true);
    radio_->setPacketReceivedAction(onDio0Isr);

    Logger::info(kTag,
                 "SX1276 pronto: %.3f MHz, BW %.0f kHz, SF%u, CR4/%u, "
                 "%d dBm",
                 freqMhz, bwKhz, cfg.spreadingFactor, cfg.codingRate,
                 cfg.txPower);
    startReceive();
    return true;
}

void LoraManager::startReceive() {
    irqFlag_ = false;
    const int16_t rc = radio_->startReceive();
    if (rc != RADIOLIB_ERR_NONE) {
        Logger::error(kTag, "startReceive fallita (codice %d)", rc);
        state_ = LoraState::Off;
        return;
    }
    state_ = LoraState::Receiving;
}

void LoraManager::loop() {
    if (state_ == LoraState::Off) {
        return;
    }

    if (state_ == LoraState::Transmitting) {
        if (irqFlag_) {
            irqFlag_ = false;
            radio_->finishTransmit();
            startReceive();
        } else if (millis() - txStartedMs_ > kTxTimeoutMs) {
            Logger::warn(kTag, "Timeout TX, torno in ricezione");
            radio_->finishTransmit();
            startReceive();
        }
        return;
    }

    // Stato Receiving
    if (!irqFlag_) {
        return;
    }
    irqFlag_ = false;

    uint8_t buf[kMaxPacket + 1];
    const size_t len = radio_->getPacketLength();
    if (len == 0 || len > kMaxPacket) {
        startReceive();
        return;
    }
    const int16_t rc = radio_->readData(buf, len);
    lastRssi_ = radio_->getRSSI();
    lastSnr_ = radio_->getSNR();
    // Riavvia subito la ricezione per non perdere pacchetti.
    startReceive();

    if (rc == RADIOLIB_ERR_NONE) {
        buf[len] = '\0';
        ++packetsReceived_;
        lastPacketMs_ = millis();
        Logger::debug(kTag, "RX %u byte, RSSI %.0f dBm, SNR %.1f dB",
                      static_cast<unsigned>(len), lastRssi_, lastSnr_);
        if (onPacket_) {
            onPacket_(buf, len, lastRssi_, lastSnr_);
        }
    } else if (rc == RADIOLIB_ERR_CRC_MISMATCH) {
        Logger::warn(kTag, "Pacchetto scartato: CRC errato");
    } else {
        Logger::warn(kTag, "Errore lettura pacchetto (codice %d)", rc);
    }
}

bool LoraManager::send(const uint8_t* data, size_t len) {
    if (state_ != LoraState::Receiving || len == 0 || len > kMaxPacket) {
        return false;
    }
    irqFlag_ = false;
    const int16_t rc =
        radio_->startTransmit(const_cast<uint8_t*>(data), len);
    if (rc != RADIOLIB_ERR_NONE) {
        Logger::warn(kTag, "startTransmit fallita (codice %d)", rc);
        startReceive();
        return false;
    }
    state_ = LoraState::Transmitting;
    txStartedMs_ = millis();
    return true;
}
