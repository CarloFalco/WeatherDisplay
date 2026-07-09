# WeatherDisplay — Gateway Meteo LoRa su LILYGO T5 4.7" ESP32-S3 E-Paper

Firmware del gateway della stazione meteo: riceve la telemetria delle
stazioni via **LoRa (SX1276, 868 MHz)**, la pubblica su **MQTT**, mostra una
dashboard sul display **e-paper 4.7"** (960×540), integra **OpenWeatherMap**
per condizioni attuali e previsioni, e distribuisce gli **aggiornamenti OTA**
ai nodi remoti scaricandoli dalle release GitHub.

## Struttura del progetto

```
src/main.cpp            wiring dei moduli e loop principale
include/version.h       versione firmware
lib/
  Logger/               log a livelli (seriale, MQTT, file LittleFS)
  ConfigManager/        configurazione JSON su LittleFS (nessun hardcode)
  WifiManager/          station + riconnessione, AP di prima configurazione
  TimeManager/          NTP, fuso orario, date italiane, fase lunare
  LoraManager/          SX1276 via RadioLib, RX/TX non bloccanti a interrupt
  NodeManager/          parsing telemetria, storico 48h, ACK, nodi offline
  MqttManager/          pubblicazione dati/stato/log, comandi, LWT
  WeatherManager/       OpenWeatherMap (attuale + previsioni 3h) in task
  OtaManager/           release GitHub → download → trasferimento LoRa
  DisplayManager/       dashboard e-paper, font 4bpp, icone, grafici
  WebPortal/            Web UI: stato, configurazione, export/import, update
tools/gen_font.py       generatore dei font bitmap (richiede Pillow)
PROTOCOL.md             protocollo radio (telemetria, ACK, OTA)
```

## Compilazione e flash

```bash
pio run                  # compila
pio run -t upload        # flash via USB
pio device monitor       # log seriale (115200)
```

## Primo avvio

Senza configurazione il gateway crea l'access point **`MeteoGateway-XXXX`**
e mostra le istruzioni sul display. Collegarsi alla rete, aprire
`http://192.168.4.1` (qualunque indirizzo reindirizza lì) e compilare
Wi-Fi, MQTT, LoRa, OpenWeatherMap. Al salvataggio il gateway riavvia e
parte in modalità normale. La configurazione è in `/config.json` su
LittleFS e sopravvive agli aggiornamenti firmware.

## Cablaggio modulo LoRa (SX1276)

Il pannello e-paper occupa GPIO **0-8, 12, 13, 38, 40, 41**: non usarli.
Pin di default (modificabili dalla Web UI, sezione *Radio LoRa → Pin*):

| Segnale | GPIO | Nota |
|---------|------|------|
| SCK  | 11 | bus SPI dello slot microSD |
| MOSI | 15 | |
| MISO | 16 | |
| CS   | 42 | |
| RST  | 17 | connettore I2C |
| DIO0 | 18 | connettore I2C |
| DIO1 | — | opzionale |

## Topic MQTT

Base configurabile (default `meteo`), `{name}` = nome gateway:

| Topic | Contenuto |
|-------|-----------|
| `meteo/gateway/{name}/status` | `online`/`offline` (retained, LWT) |
| `meteo/gateway/{name}/info` | fw, IP, RSSI, uptime, heap, contatori (retained) |
| `meteo/gateway/{name}/log` | log del firmware (se abilitato) |
| `meteo/gateway/{name}/ota` | stato OTA nodi (retained) |
| `meteo/node/{id}/data` | telemetria stazione + RSSI/SNR |
| `meteo/gateway/{name}/cmd/restart` | riavvio |
| `meteo/gateway/{name}/cmd/refresh` | refresh forzato del display |
| `meteo/gateway/{name}/cmd/info` | pubblica subito lo stato |
| `meteo/gateway/{name}/cmd/ota_check` | controlla le release GitHub |
| `meteo/gateway/{name}/cmd/ota_confirm` | payload = id nodo da aggiornare |

## Display

Dashboard ricalcata su `DashboardEsempio.png`: header con stazione corrente
(nome + batteria + ultimo aggiornamento + RSSI), data/ora e icone di stato;
bussola del vento; temperatura/umidità grandi (sensori locali quando
disponibili, altrimenti OpenWeatherMap); alba/tramonto e fase lunare;
previsioni a 3 ore su 7 slot; quattro grafici storici a 48 h (pressione,
temperatura, umidità, pioggia). Con più stazioni i dati ruotano una
stazione alla volta (intervallo configurabile). Il refresh avviene solo al
cambio di minuto, alla rotazione o all'arrivo di dati nuovi. Le anomalie
(Wi-Fi giù, broker irraggiungibile, nodo offline, OTA in corso) compaiono
in una barra eventi sotto l'header.

## OTA

- **Nodi**: il gateway controlla periodicamente l'ultima release di
  `CarloFalco/WeatherStation`; se più recente del firmware di un nodo la
  segnala su MQTT e Web UI. Alla conferma scarica `firmware.bin` su
  LittleFS e lo trasferisce via LoRa (blocchi numerati, ACK a finestre,
  ritrasmissione, CRC32 finale, ripresa da interruzione). Dettagli in
  [PROTOCOL.md](PROTOCOL.md).
- **Gateway**: upload del `.bin` dalla Web UI (sezione *Manutenzione*)
  oppure ArduinoOTA (`pio run -t upload --upload-port <IP>`). La
  configurazione su LittleFS non viene toccata.

## Font del display

Gli header in `lib/DisplayManager/fonts/` sono generati da
`tools/gen_font.py` (Segoe UI 16/22/26/64 px, 4 bpp, charset ASCII +
accentate italiane + `°`). Per rigenerarli: `python tools/gen_font.py`.

## Note

- Le connessioni HTTPS (OpenWeatherMap, GitHub) usano `setInsecure()`:
  niente validazione del certificato. Accettabile per dati meteo pubblici;
  da irrobustire se il canale diventa sensibile.
- Le richieste HTTPS e i download girano in task FreeRTOS dedicati sul
  core 0: il loop principale non si blocca mai.
