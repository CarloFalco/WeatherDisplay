# Protocollo radio LoRa — Gateway ↔ Stazioni

Tutti i messaggi di controllo sono JSON in chiaro (payload LoRa ≤ 255 byte).
Fanno eccezione i blocchi firmware OTA, trasmessi in binario (vedi sotto).
Il canale è condiviso: ogni messaggio contiene l'`id` della stazione e il
ricevitore deve sempre verificarlo.

## Telemetria (stazione → gateway)

```json
{"type":"data","id":"ws-01","fw":"2.1.0","seq":123,
 "t":21.4,"rh":63.0,"p":1013.2,"rain":0.6,
 "ws":3.2,"wg":5.1,"wd":270,
 "vbat":3.98,"ibat":-52,"ipan":120,"iload":35}
```

| Campo | Tipo | Unità | Descrizione |
|-------|------|-------|-------------|
| `type` | string | — | `"data"` = telemetria |
| `id` | string | — | Identificativo stazione (`ws-01`, ...) |
| `fw` | string | — | Versione firmware SemVer |
| `seq` | uint | — | Contatore progressivo (rollover 65535), per rilevare perdite |
| `t` | float | °C | Temperatura |
| `rh` | float | % | Umidità relativa |
| `p` | float | hPa | Pressione |
| `rain` | float | mm | Pioggia accumulata dall'ultimo invio riuscito |
| `ws` | float | m/s | Vento medio |
| `wg` | float | m/s | Raffica |
| `wd` | uint | ° | Direzione vento (0 = N, orario) |
| `vbat` | float | V | Tensione batteria |
| `ibat` | int | mA | Corrente batteria (+ = carica) |
| `ipan` | int | mA | Corrente pannello |
| `iload` | int | mA | Corrente carico |

I campi possono mancare (sensore guasto o non implementato): il gateway
mantiene l'ultimo valore noto per le grandezze istantanee e considera
`rain` solo se presente nel pacchetto.

## ACK (gateway → stazione)

Inviato dal gateway a ogni pacchetto `data` valido:

```json
{"type":"ack","id":"ws-01","seq":123}
```

## Aggiornamento OTA del nodo

Il firmware proviene dall'ultima release GitHub del repository configurato
(default `CarloFalco/WeatherStation`), asset `firmware.bin`. La procedura
parte solo dopo conferma dell'utente (Web UI o MQTT).

### 1. Avvio (gateway → nodo)

```json
{"type":"ota_start","id":"ws-01","ver":"2.2.0","size":481280,
 "crc":305419896,"chunks":2674,"cs":180,"win":8}
```

- `size`: dimensione firmware in byte
- `crc`: CRC32 (poly riflesso `0xEDB88320`) dell'intero file
- `chunks`: numero blocchi, `cs`: payload per blocco, `win`: blocchi tra due ACK

### 2. ACK di avanzamento (nodo → gateway)

```json
{"type":"ota_ack","id":"ws-01","next":0}
```

`next` è il primo blocco che il nodo si aspetta. Serve sia da risposta
all'`ota_start` (0 per iniziare, oppure l'indice da cui **riprendere** un
aggiornamento interrotto, se il nodo ha già dati validi in flash), sia da
conferma di ogni finestra: dopo `win` blocchi il gateway attende un
`ota_ack`; se `next` è arretrato ritrasmette da lì.

Timeout gateway: 15 s, massimo 5 ritrasmissioni, poi la procedura fallisce
(riprendibile con una nuova conferma).

### 3. Blocchi firmware (gateway → nodo, binari)

```
byte 0   0xFA        magic
byte 1   0x50        magic
byte 2-3 seq         indice blocco, little-endian
byte 4   len         byte di payload (≤ 180)
byte 5.. payload
```

L'integrità del singolo pacchetto è garantita dal CRC hardware LoRa.

### 4. Chiusura

Quando `next == chunks` il gateway invia:

```json
{"type":"ota_end","id":"ws-01","crc":305419896}
```

Il nodo verifica il CRC32 dell'immagine completa e risponde:

```json
{"type":"ota_done","id":"ws-01","ok":true}
```

`ok:false` (o assenza di risposta) = procedura fallita; il gateway pubblica
l'esito su MQTT (`{base}/gateway/{name}/ota`).
