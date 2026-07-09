# Specifica tecnica - Gateway Meteo LoRa con LILYGO T5 4.7" ESP32-S3

## Obiettivo

Realizzare il firmware di una stazione gateway basata su **LILYGO T5 4.7" ESP32-S3 E-Paper**, con il compito di:

* ricevere i dati dalle stazioni meteo tramite LoRa;
* pubblicare i dati su un broker MQTT tramite Wi-Fi;
* visualizzare le informazioni sul display e-paper;
* gestire la configurazione tramite interfaccia Web;
* gestire gli aggiornamenti OTA delle stazioni remote.

Il codice dovrà essere scritto in **C++ utilizzando PlatformIO** e seguire una struttura modulare, facilmente estendibile e manutenibile.

---

# Hardware

## Scheda

* LILYGO T5 4.7" ESP32-S3 E-Paper
* ESP32-S3
* Wi-Fi
* Bluetooth (non utilizzato)
* Display e-paper 4.7"
* Memoria Flash
* LittleFS
* eventuale microSD (opzionale)
* Modulo LoRa esterno o integrato (in base alla revisione hardware)

Il display **non è touch**.

---

# Funzionalità principali

Il gateway dovrà svolgere contemporaneamente le seguenti funzioni:

* Ricezione pacchetti LoRa
* Decodifica dei dati provenienti dai nodi
* Pubblicazione MQTT
* Gestione OTA dei nodi
* Aggiornamento del display
* Sincronizzazione oraria NTP
* Configurazione tramite Web
* Logging degli eventi
* Monitoraggio dello stato del sistema

---

# Architettura software

Organizzare il progetto in moduli indipendenti.

Esempio:

```text
src/
    main.cpp

lib/
    ConfigManager/
    WifiManager/
    WebPortal/
    MqttManager/
    LoraManager/
    DisplayManager/
    NodeManager/
    OtaManager/
    TimeManager/
    Logger/
```

Ogni modulo dovrà avere:

* file .h
* file .cpp
* responsabilità ben definita
* nessuna dipendenza circolare

---

# Configurazione

Non hardcodare alcun parametro.

La configurazione dovrà essere salvata su **LittleFS** in formato JSON.

Esempio:

```json
{
    "wifi":{
        "ssid":"",
        "password":""
    },
    "mqtt":{
        "host":"",
        "port":1883,
        "username":"",
        "password":"",
        "base_topic":"meteo"
    },
    "lora":{
        "frequency":868000000,
        "bandwidth":125000,
        "spreading_factor":9,
        "coding_rate":5,
        "tx_power":20
    },
    "gateway":{
        "name":"Gateway01",
        "timezone":"Europe/Rome"
    }
}
```

---

# Primo avvio

Se il file di configurazione non esiste:

* creare un Access Point Wi-Fi
* SSID:

```
MeteoGateway-XXXX
```

* avviare automaticamente un Web Server
* mostrare una pagina di configurazione

L'utente dovrà poter configurare:

* Wi-Fi
* Broker MQTT
* Parametri LoRa
* Nome gateway
* Timezone

Alla conferma:

* salvare il JSON
* riavviare il dispositivo

---

# Pagina Web

La Web UI dovrà permettere di:

* visualizzare la configurazione corrente
* modificare la configurazione
* riavviare il dispositivo
* visualizzare lo stato Wi-Fi
* visualizzare lo stato MQTT
* visualizzare lo stato LoRa
* visualizzare i nodi collegati
* visualizzare la versione firmware
* esportare/importare la configurazione
* avviare manualmente il controllo OTA

---

# MQTT

Pubblicare:

* dati delle stazioni
* stato del gateway
* versione firmware
* RSSI LoRa
* uptime
* memoria disponibile

Sottoscrivere topic dedicati ai comandi:

* riavvio
* refresh display
* richiesta informazioni
* avvio aggiornamento OTA

---

# Ricezione LoRa



---

# Display E-Paper

Il display dovrà mostrare almeno:

## Stato gateway

* ora
* data
* Wi-Fi
* MQTT
* LoRa

## Elenco nodi

Per ogni nodo:

* nome
* temperatura
* umidità
* batteria
* ultimo aggiornamento
* RSSI

## Eventi

Mostrare eventuali:

* errore Wi-Fi
* broker MQTT non raggiungibile
* nodo offline
* aggiornamento OTA in corso

Aggiornare il display solo quando necessario per ridurre il numero di refresh.

---

# Integrazione OpenWeatherMap

Il gateway dovrà integrare il servizio OpenWeatherMap API per ottenere le condizioni meteo attuali e le previsioni esterne alla rete dei sensori LoRa.

La connessione dovrà avvenire tramite HTTPS utilizzando il Wi-Fi del gateway.

Parametri di connessione:

const char* host = "api.openweathermap.org";
const int httpsPort = 443;

La richiesta API dovrà essere generata dinamicamente utilizzando:

String addr = 
"https://api.openweathermap.org/data/2.5/weather?q="
+ city_name +
"&appid="
+ owkey;
## Configurazione OpenWeatherMap

I parametri non devono essere hardcoded.

Devono essere salvati nella configurazione JSON su LittleFS.

Esempio:

{
    "weather_api":{
        "enabled":true,
        "city":"Padova",
        "country":"IT",
        "api_key":"",
        "update_interval":1800
    }
}

Parametri:

enabled → abilita/disabilita il servizio
city → città richiesta
country → codice paese ISO
api_key → chiave OpenWeatherMap
update_interval → intervallo aggiornamento in secondi
## Gestione dati meteo esterni

Il gateway dovrà periodicamente:

collegarsi al servizio OpenWeatherMap;
scaricare i dati JSON;
analizzare la risposta;
aggiornare lo stato interno;
aggiornare il display e-paper.

Dati da visualizzare:

temperatura attuale
temperatura percepita
condizioni meteo
umidità
pressione atmosferica
velocità vento
icona meteo
descrizione condizioni
eventuale previsione futura

---

# OTA nodi LoRa

Il gateway dovrà:

* controllare periodicamente la disponibilità di nuovi firmware
* notificare la disponibilità dell'aggiornamento tramite MQTT
* attendere la conferma dell'utente
* scaricare il firmware
* memorizzarlo localmente
* inviarlo via LoRa

Il protocollo OTA dovrà prevedere:

* pacchetti numerati
* ACK
* ritrasmissione automatica
* verifica CRC finale
* possibilità di riprendere un aggiornamento interrotto

---
# Display grafico. 
ho caricato un immagine di esempio nella cartella di progetto oltre alle cose mostrate nel Esempio
vorrei che se ci sono piu stazioni connesse potesse mostrare i dati delle varie stazioni a rotazione una alla volta
quando mostra i dati di una stazione mostra anche lo stato della batteria di quella stazione

Nome Stazione 🔋90%

---

# Json per ricezione e invio informazioni

## Messaggio di telemetria (stazione → base)

Un oggetto JSON compatto (senza spazi né newline) per ogni ciclo di misura:

```json
{"type":"data","id":"ws-01","fw":"2.1.0","seq":123,"t":21.4,"rh":63.0,"p":1013.2,"rain":0.6,"ws":3.2,"wg":5.1,"wd":270,"vbat":3.98,"ibat":-52,"ipan":120,"iload":35}
```

| Campo | Tipo | Unità | Sorgente | Descrizione |
|-------|------|-------|----------|-------------|
| `type` | string | — | — | `"data"` = telemetria (futuri: `"ack"`, `"ota"`, ...) |
| `id` | string | — | config.ini | Identificativo stazione (`ws-01`, `ws-02`, ...) |
| `fw` | string | — | version.h | Versione firmware SemVer |
| `seq` | uint | — | RTC RAM | Contatore progressivo messaggi (rollover a 65535); per rilevare messaggi persi e correlare gli ACK |
| `t` | float | °C | BME280 | Temperatura, 1 decimale |
| `rh` | float | % | BME280 | Umidità relativa, 1 decimale |
| `p` | float | hPa | BME280 | Pressione (a livello stazione), 1 decimale |
| `rain` | float | mm | Pluviometro | Pioggia accumulata dall'ultimo invio riuscito |
| `ws` | float | m/s | Anemometro | Velocità vento media nella finestra di campionamento |
| `wg` | float | m/s | Anemometro | Raffica (massimo nella finestra) |
| `wd` | uint | ° | AS5600 | Direzione vento, 0–359 (0 = Nord, senso orario) |
| `vbat` | float | V | INA3221 ch2 | Tensione batteria |
| `ibat` | int | mA | INA3221 ch2 | Corrente batteria (positiva = carica, negativa = scarica) |
| `ipan` | int | mA | INA3221 ch1 | Corrente pannello solare |
| `iload` | int | mA | INA3221 ch3 | Corrente assorbita dal carico |

Note per il ricevitore:

- Campi **assenti** = sensore guasto o non ancora implementato: il parser
  non deve assumere che ci siano tutti (durante lo sviluppo arriveranno
  prima `t/rh/p`, poi via via gli altri).
- `id` va sempre controllato: più stazioni condividono lo stesso canale.
- Fino all'increment "protocollo" la stazione trasmette senza attendere
  ACK (fire-and-forget): per il primo debug basta stampare i pacchetti.

## ACK (base → stazione) — bozza, non ancora implementato

```json
{"type":"ack","id":"ws-01","seq":123}
```


# Logging

Implementare livelli:

* ERROR
* WARNING
* INFO
* DEBUG

Possibilità di:

* inviare log sulla seriale
* pubblicare log via MQTT
* salvare log su LittleFS (opzionale)

---

# Robustezza

Il firmware dovrà essere resiliente.

In caso di perdita:

* Wi-Fi
* MQTT
* LoRa

dovrà tentare automaticamente la riconnessione senza riavviare il dispositivo.

---

# Aggiornamento firmware gateway

Il firmware del gateway dovrà supportare OTA via Wi-Fi.

L'aggiornamento del gateway non dovrà cancellare la configurazione salvata su LittleFS.

---

# Requisiti di sviluppo

Seguire le seguenti linee guida:

* C++ moderno
* PlatformIO
* classi ben separate
* basso utilizzo della RAM
* evitare variabili globali
* utilizzo di `constexpr`
* utilizzo di `enum class`
* RAII quando possibile
* commenti Doxygen
* codice facilmente testabile
* naming convention coerente
* gestione centralizzata degli errori
* nessun blocco del `loop()`
* utilizzo di macchine a stati per le operazioni lunghe
* evitare `delay()`, utilizzare timer non bloccanti (`millis()`)

---

# Obiettivo finale

Realizzare un gateway stabile, modulare e facilmente estendibile, capace di gestire numerose stazioni meteo LoRa, pubblicare i dati su MQTT, offrire una semplice interfaccia di configurazione e distribuire aggiornamenti OTA ai nodi remoti in modo affidabile.
