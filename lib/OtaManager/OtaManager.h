/**
 * @file OtaManager.h
 * @brief Aggiornamento OTA dei nodi LoRa da release GitHub.
 *
 * Flusso:
 *  1. Controllo periodico dell'ultima release del repository configurato.
 *  2. Se la versione è più recente del firmware di almeno un nodo, la
 *     disponibilità viene notificata via MQTT e Web UI.
 *  3. Alla conferma dell'utente il firmware viene scaricato su LittleFS
 *     (con CRC32) e trasferito al nodo via LoRa a blocchi numerati con
 *     ACK, ritrasmissione, verifica CRC finale e possibilità di ripresa.
 *
 * Dettagli del protocollo radio in PROTOCOL.md.
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <atomic>
#include <functional>

#include "ConfigManager.h"
#include "LoraManager.h"
#include "NodeManager.h"

/** @brief Stato del processo OTA nodi. */
enum class OtaState : uint8_t {
    Idle,          ///< Nessuna attività.
    Checking,      ///< Interrogazione GitHub in corso.
    UpToDate,      ///< Nessun aggiornamento necessario.
    Available,     ///< Aggiornamento disponibile, in attesa di conferma.
    Downloading,   ///< Download del firmware in corso.
    Ready,         ///< Firmware scaricato e verificato.
    Starting,      ///< Inviato ota_start, in attesa del primo ACK.
    Transferring,  ///< Invio blocchi in corso.
    Ending,        ///< Inviato ota_end, in attesa di ota_done.
    Success,       ///< Nodo aggiornato con successo.
    Error,         ///< Procedura fallita.
};

/**
 * @brief Coordinatore dell'OTA dei nodi remoti.
 */
class OtaManager {
   public:
    using StatusFn = std::function<void(const String& json)>;

    /** @brief Dimensione payload di ogni blocco firmware via LoRa. */
    static constexpr size_t kChunkPayload = 180;
    /** @brief Blocchi inviati tra un ACK e il successivo. */
    static constexpr uint16_t kWindowSize = 8;

    void begin(const NodeOtaConfig& cfg, LoraManager& lora,
               NodeManager& nodes);

    /** @brief Da chiamare a ogni giro di loop(). */
    void loop(bool networkUp);

    /** @brief Avvia subito un controllo release (Web UI / MQTT). */
    void checkNow();

    /**
     * @brief Conferma dell'utente: scarica (se serve) e aggiorna il nodo.
     * @param nodeId Stazione da aggiornare.
     */
    void confirm(const String& nodeId);

    /** @brief Annulla la procedura e torna in Idle. */
    void abort(const char* reason);

    /** @brief Gestisce i messaggi LoRa type=ota_* (da NodeManager). */
    void handleNodeMessage(JsonDocument& doc);

    OtaState state() const { return state_; }
    const char* stateName() const;
    String latestVersion() const { return latestVersion_; }
    /** @brief true se c'è un trasferimento in corso (per il display). */
    bool inProgress() const {
        return state_ >= OtaState::Downloading && state_ <= OtaState::Ending;
    }
    /** @brief Avanzamento 0-100 del trasferimento. */
    int progressPct() const;

    /** @brief JSON di stato per MQTT/Web UI. */
    String statusJson() const;

    /** @brief Callback su ogni cambio di stato (pubblicazione MQTT). */
    void onStatus(StatusFn cb) { onStatus_ = cb; }

    /** @brief CRC32 (poly riflesso 0xEDB88320). */
    static uint32_t crc32(uint32_t crc, const uint8_t* data, size_t len);

   private:
    static void checkTaskEntry(void* self);
    static void downloadTaskEntry(void* self);

    void setState(OtaState s);
    void runCheck();
    void runDownload();
    void startTransfer();
    void sendChunk(uint16_t idx);
    void sendStart();
    void sendEnd();

    NodeOtaConfig cfg_;
    LoraManager* lora_ = nullptr;
    NodeManager* nodes_ = nullptr;

    OtaState state_ = OtaState::Idle;
    StatusFn onStatus_;
    String lastError_;

    // Risultati del task di check (letti a flag alzato).
    std::atomic<bool> taskRunning_{false};
    std::atomic<bool> checkDone_{false};
    std::atomic<bool> downloadDone_{false};
    bool taskOk_ = false;
    String latestVersion_;
    String assetUrl_;
    size_t assetSize_ = 0;

    // Firmware scaricato.
    size_t fwSize_ = 0;
    uint32_t fwCrc_ = 0;
    uint16_t totalChunks_ = 0;

    // Trasferimento verso il nodo.
    String targetNode_;
    uint16_t nextChunk_ = 0;      ///< primo blocco non ancora confermato
    uint16_t windowSent_ = 0;     ///< blocchi inviati nella finestra corrente
    uint8_t retries_ = 0;
    uint32_t lastActionMs_ = 0;
    uint32_t nextCheckMs_ = 0;  ///< millis() del prossimo controllo release

    static constexpr uint8_t kMaxRetries = 5;
    static constexpr uint32_t kAckTimeoutMs = 15000;
    static constexpr const char* kFwFile = "/node_fw.bin";
};
