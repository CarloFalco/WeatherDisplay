/**
 * @file NodeManager.h
 * @brief Anagrafica e stato delle stazioni meteo LoRa.
 *
 * Decodifica i pacchetti JSON delle stazioni (vedi PROTOCOL.md), mantiene
 * l'ultimo valore di ogni grandezza, uno storico di 48 ore per i grafici e
 * rileva i nodi offline. I campi assenti nel pacchetto restano NAN.
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <functional>
#include <vector>

/** @brief Numero di slot dello storico (48h a passi di 30 minuti). */
constexpr size_t kHistorySlots = 96;
/** @brief Durata di uno slot dello storico in secondi. */
constexpr uint32_t kHistorySlotSeconds = 1800;

/**
 * @brief Storico circolare di una grandezza per i grafici.
 */
struct History {
    float temp[kHistorySlots];
    float rh[kHistorySlots];
    float press[kHistorySlots];
    float rain[kHistorySlots];
    uint32_t slotEpoch[kHistorySlots];  ///< Epoch/1800 dello slot (0 = vuoto)
    size_t head = 0;                    ///< Indice dello slot corrente

    History();

    /** @brief Aggiunge un campione (crea nuovi slot se il tempo è avanzato). */
    void addSample(time_t now, float t, float h, float p, float rainDelta);

    /**
     * @brief Estrae una serie in ordine cronologico (vecchio -> nuovo).
     * @param which 0=temp, 1=rh, 2=press, 3=rain
     * @param out   Array di kHistorySlots float (NAN per slot vuoti).
     */
    void series(int which, float* out) const;
};

/**
 * @brief Stato corrente di una stazione.
 */
struct NodeInfo {
    String id;
    String fw;
    uint16_t seq = 0;
    uint32_t packetsReceived = 0;
    uint32_t packetsLost = 0;

    uint32_t lastSeenMs = 0;    ///< millis() ultimo pacchetto
    time_t lastSeenEpoch = 0;   ///< epoch ultimo pacchetto (0 se NTP assente)
    float rssi = 0;
    float snr = 0;
    bool offline = false;

    // Telemetria (NAN = mai ricevuta / sensore assente)
    float t = NAN;      ///< °C
    float rh = NAN;     ///< %
    float p = NAN;      ///< hPa
    float rain = NAN;   ///< mm dall'ultimo invio
    float ws = NAN;     ///< m/s medio
    float wg = NAN;     ///< m/s raffica
    int wd = -1;        ///< ° (0=N), -1 = assente
    float vbat = NAN;   ///< V
    float ibat = NAN;   ///< mA (+ carica)
    float ipan = NAN;   ///< mA pannello
    float iload = NAN;  ///< mA carico

    History history;

    /** @brief Percentuale batteria stimata da vbat (LiPo); -1 se ignota. */
    int batteryPct() const;
};

/**
 * @brief Gestisce l'elenco delle stazioni e i loro dati.
 */
class NodeManager {
   public:
    using DataFn = std::function<void(const NodeInfo& node)>;
    using OfflineFn = std::function<void(const NodeInfo& node, bool offline)>;
    using AckFn = std::function<void(const String& id, uint16_t seq)>;
    using OtaMsgFn = std::function<void(JsonDocument& doc)>;

    /** @param nodeTimeoutS Secondi senza pacchetti prima di dichiarare offline. */
    explicit NodeManager(uint32_t nodeTimeoutS = 900) : timeoutS_(nodeTimeoutS) {}

    /** @brief Aggiorna il timeout offline (dalla configurazione). */
    void setTimeout(uint32_t seconds) { timeoutS_ = seconds; }

    /**
     * @brief Elabora un pacchetto LoRa ricevuto.
     * @return true se il pacchetto era valido.
     */
    bool handlePacket(const uint8_t* data, size_t len, float rssi, float snr,
                      time_t nowEpoch);

    /** @brief Rilevazione nodi offline; da chiamare periodicamente. */
    void loop();

    const std::vector<NodeInfo>& nodes() const { return nodes_; }
    size_t count() const { return nodes_.size(); }

    NodeInfo* find(const String& id);

    /** @brief Serializza lo stato di un nodo per la pubblicazione MQTT. */
    static String toJson(const NodeInfo& node);

    void onData(DataFn cb) { onData_ = cb; }
    void onOfflineChange(OfflineFn cb) { onOffline_ = cb; }
    /** @brief Registrata da main per inviare gli ACK via LoRa. */
    void setAckSender(AckFn cb) { ackSender_ = cb; }
    /** @brief Messaggi type=ota_* vengono inoltrati qui (OtaManager). */
    void onOtaMessage(OtaMsgFn cb) { onOtaMsg_ = cb; }

   private:
    NodeInfo& findOrCreate(const String& id);

    uint32_t timeoutS_;
    std::vector<NodeInfo> nodes_;
    DataFn onData_;
    OfflineFn onOffline_;
    AckFn ackSender_;
    OtaMsgFn onOtaMsg_;
};
