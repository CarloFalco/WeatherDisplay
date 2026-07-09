/**
 * @file DisplayManager.cpp
 * @brief Implementazione del rendering della dashboard.
 */
#include "DisplayManager.h"

#include <Logger.h>
#include <epd_driver.h>
#include <esp_heap_caps.h>

#include <cmath>

#include "Gfx/Icons.h"
#include "fonts/font_big.h"
#include "fonts/font_bold.h"
#include "fonts/font_med.h"
#include "fonts/font_small.h"

namespace {
constexpr const char* kTag = "Display";
constexpr uint8_t kInk = 0;
constexpr uint8_t kGrayLight = 200;
constexpr uint8_t kGrayMid = 128;

/** @brief Formatta un float con 1 decimale, "-" se NAN. */
String num1(float v) { return isnan(v) ? String("-") : String(v, 1); }
/** @brief Formatta un float senza decimali, "-" se NAN. */
String num0(float v) { return isnan(v) ? String("-") : String(v, 0); }
}  // namespace

bool DisplayManager::begin(const DisplayConfig& cfg, NodeManager& nodes,
                           WeatherManager& weather, TimeManager& time) {
    cfg_ = cfg;
    nodes_ = &nodes;
    weather_ = &weather;
    time_ = &time;

    epd_init();
    fb_ = static_cast<uint8_t*>(heap_caps_malloc(
        EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (fb_ == nullptr) {
        Logger::error(kTag, "Allocazione framebuffer fallita");
        return false;
    }
    gfx_ = new Gfx(fb_, EPD_WIDTH, EPD_HEIGHT);
    gfx_->clear();
    ready_ = true;
    Logger::info(kTag, "Display inizializzato (%dx%d)", EPD_WIDTH,
                 EPD_HEIGHT);
    return true;
}

void DisplayManager::requestRefresh() {
    dirty_ = true;
    forceNow_ = true;
}

void DisplayManager::loop() {
    if (!ready_) {
        return;
    }

    const uint32_t now = millis();

    // Rotazione tra stazioni.
    if (nodes_->count() > 1 &&
        now - lastRotationMs_ >= cfg_.rotationInterval * 1000UL) {
        lastRotationMs_ = now;
        rotationIndex_ = (rotationIndex_ + 1) % nodes_->count();
        dirty_ = true;
    }

    // Cambio di minuto (orologio in header).
    struct tm tm{};
    if (time_->localTime(tm) && tm.tm_min != lastMinute_) {
        lastMinute_ = tm.tm_min;
        dirty_ = true;
    }

    if (!dirty_) {
        return;
    }
    // Limita la frequenza dei refresh per preservare il pannello.
    if (!forceNow_ && now - lastRefreshMs_ < kMinRefreshMs) {
        return;
    }
    dirty_ = false;
    forceNow_ = false;
    lastRefreshMs_ = now;
    render();
}

void DisplayManager::flush() {
    epd_poweron();
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), fb_);
    epd_poweroff();
    ++refreshCount_;
}

void DisplayManager::render() {
    const GatewayStatus st = status_ ? status_() : GatewayStatus{};
    const NodeInfo* node = nullptr;
    if (nodes_->count() > 0) {
        rotationIndex_ %= nodes_->count();
        node = &nodes_->nodes()[rotationIndex_];
    }

    gfx_->clear();
    drawHeader(st, node);
    drawEvents(st);
    drawCompass(node);
    drawSunMoon();
    drawCurrent(node);
    drawForecast();
    drawGraphs(node);
    flush();
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------

void DisplayManager::drawHeader(const GatewayStatus& st,
                                const NodeInfo* node) {
    // Sinistra: nome stazione + batteria.
    int x = 12;
    if (node != nullptr) {
        gfx_->text(x, 8, node->id.c_str(), FontBold);
        x += gfx_->textWidth(node->id.c_str(), FontBold) + 14;
        const int pct = node->batteryPct();
        icons::battery(*gfx_, x, 14, 36, 18, pct);
        x += 42;
        String pctStr = pct >= 0 ? String(pct) + "%" : String("-");
        gfx_->text(x, 12, pctStr.c_str(), FontMed);
        x += gfx_->textWidth(pctStr.c_str(), FontMed) + 16;
        // Ultimo aggiornamento e RSSI.
        String info;
        if (node->lastSeenEpoch > 0) {
            info += "agg " + time_->timeShortFrom(node->lastSeenEpoch);
        }
        info += "  " + String(node->rssi, 0) + "dBm";
        gfx_->text(x, 16, info.c_str(), FontSmall, kGrayMid);
    } else {
        gfx_->text(x, 8, "In attesa di stazioni...", FontBold, kGrayMid);
    }

    // Destra: icone di stato.
    int ix = kW - 30;
    icons::lora(*gfx_, ix - 22, 10, 26, st.loraReady);
    ix -= 36;
    icons::mqtt(*gfx_, ix - 26, 10, 26, st.mqttConnected);
    ix -= 36;
    icons::wifi(*gfx_, ix - 26, 10, 26, st.wifiConnected);
    ix -= 40;

    // Data e ora a destra delle icone.
    const String dt = time_->dateitalian() + "  " + time_->timeShort();
    gfx_->text(ix, 12, dt.c_str(), FontMed, kInk, TextAlign::Right);

    gfx_->hline(0, kHeaderH - 1, kW, kInk);
}

// ---------------------------------------------------------------------------
// Eventi
// ---------------------------------------------------------------------------

void DisplayManager::drawEvents(const GatewayStatus& st) {
    String events;
    auto add = [&events](const String& e) {
        if (events.length() > 0) {
            events += "   ";
        }
        events += "! " + e;
    };

    if (st.apMode) {
        add("Modalita configurazione attiva");
    } else if (!st.wifiConnected) {
        add("Wi-Fi disconnesso");
    }
    if (st.wifiConnected && st.mqttConfigured && !st.mqttConnected) {
        add("Broker MQTT non raggiungibile");
    }
    if (!st.loraReady) {
        add("Radio LoRa non disponibile");
    }
    for (const NodeInfo& n : nodes_->nodes()) {
        if (n.offline) {
            add("Nodo " + n.id + " offline");
        }
    }
    if (st.otaInProgress) {
        add("OTA " + st.otaNode + " " + String(st.otaProgress) + "%");
    }
    if (st.weatherFailed) {
        add("Meteo esterno non aggiornato");
    }

    if (events.length() == 0) {
        return;
    }
    gfx_->fillRect(0, kHeaderH, kW, kEventH, 32);
    gfx_->text(kW / 2, kHeaderH + 4, events.c_str(), FontSmall, 255,
               TextAlign::Center);
}

// ---------------------------------------------------------------------------
// Bussola del vento
// ---------------------------------------------------------------------------

const char* DisplayManager::cardinal(int deg) {
    static const char* kNames[] = {"N",  "NE", "E",  "SE",
                                   "S",  "SO", "O",  "NO"};
    if (deg < 0) {
        return "-";
    }
    return kNames[((deg + 22) / 45) % 8];
}

void DisplayManager::drawCompass(const NodeInfo* node) {
    const int cx = kLeftColW / 2;
    const int cy = kMainTop + 82;
    const int r = 76;

    gfx_->ring(cx, cy, r, 3, kInk);
    // Tacche ogni 45 gradi.
    for (int a = 0; a < 360; a += 45) {
        const float rad = (a - 90) * static_cast<float>(M_PI) / 180.0f;
        const int x0 = cx + static_cast<int>(cosf(rad) * (r - 10));
        const int y0 = cy + static_cast<int>(sinf(rad) * (r - 10));
        const int x1 = cx + static_cast<int>(cosf(rad) * (r - 3));
        const int y1 = cy + static_cast<int>(sinf(rad) * (r - 3));
        gfx_->lineThick(x0, y0, x1, y1, 2, kInk);
    }
    // Cardinali esterni.
    gfx_->text(cx, cy - r - 20, "N", FontSmall, kInk, TextAlign::Center);
    gfx_->text(cx, cy + r + 4, "S", FontSmall, kInk, TextAlign::Center);
    gfx_->text(cx + r + 12, cy - 8, "E", FontSmall, kInk, TextAlign::Center);
    gfx_->text(cx - r - 12, cy - 8, "O", FontSmall, kInk, TextAlign::Center);

    const bool hasWind = node != nullptr && !isnan(node->ws);
    const int wd = node != nullptr ? node->wd : -1;

    // Freccia della direzione (da dove viene il vento).
    if (wd >= 0) {
        const float rad = (wd - 90) * static_cast<float>(M_PI) / 180.0f;
        const int tx = cx + static_cast<int>(cosf(rad) * (r - 14));
        const int ty = cy + static_cast<int>(sinf(rad) * (r - 14));
        const float lrad = rad + 2.7f;
        const float rrad = rad - 2.7f;
        const int lx = tx + static_cast<int>(cosf(lrad) * 16);
        const int ly = ty + static_cast<int>(sinf(lrad) * 16);
        const int rx = tx + static_cast<int>(cosf(rrad) * 16);
        const int ry = ty + static_cast<int>(sinf(rrad) * 16);
        gfx_->fillTriangle(tx, ty, lx, ly, rx, ry, kInk);
    }

    // Centro: cardinale, velocità, unità, gradi.
    gfx_->text(cx, cy - 52, cardinal(wd), FontMed, kInk, TextAlign::Center);
    gfx_->text(cx, cy - 30, num1(hasWind ? node->ws : NAN).c_str(), FontBold,
               kInk, TextAlign::Center);
    gfx_->text(cx, cy + 2, "m/s", FontSmall, kGrayMid, TextAlign::Center);
    if (wd >= 0) {
        String degs = String(wd) + "\xC2\xB0";
        gfx_->text(cx, cy + 20, degs.c_str(), FontSmall, kGrayMid,
                   TextAlign::Center);
    }

    // Raffica sotto la bussola.
    if (node != nullptr && !isnan(node->wg)) {
        String gust = "raffica " + num1(node->wg) + " m/s";
        gfx_->text(cx, cy + r + 22, gust.c_str(), FontSmall, kInk,
                   TextAlign::Center);
    }
}

// ---------------------------------------------------------------------------
// Alba/tramonto e fase lunare
// ---------------------------------------------------------------------------

void DisplayManager::drawSunMoon() {
    const CurrentWeather& w = weather_->current();
    const int y = kForecastTop - 76;

    if (w.valid && w.sunrise > 0) {
        String rise = time_->timeShortFrom(w.sunrise) + " Alba";
        String set = time_->timeShortFrom(w.sunset) + " Tramonto";
        gfx_->text(14, y, rise.c_str(), FontMed);
        gfx_->text(14, y + 26, set.c_str(), FontMed);
    }

    const time_t now = time_->now();
    if (now > 0) {
        const float phase = TimeManager::moonPhase(now);
        icons::moon(*gfx_, kLeftColW - 44, y + 26, 24, phase);
        gfx_->text(14, y + 52, TimeManager::moonPhaseName(phase), FontSmall,
                   kGrayMid);
    }
}

// ---------------------------------------------------------------------------
// Condizioni attuali
// ---------------------------------------------------------------------------

void DisplayManager::drawCurrent(const NodeInfo* node) {
    const CurrentWeather& w = weather_->current();
    const int x0 = kLeftColW + 28;
    int y = kMainTop + 6;

    // Temperatura e umidità: privilegia i sensori locali, poi OWM.
    const float temp =
        (node != nullptr && !isnan(node->t)) ? node->t
                                             : (w.valid ? w.temp : NAN);
    const float rh = (node != nullptr && !isnan(node->rh))
                         ? node->rh
                         : (w.valid ? static_cast<float>(w.humidity) : NAN);

    String tempStr = num1(temp) + "\xC2\xB0";
    gfx_->text(x0, y, tempStr.c_str(), FontBig);
    const int tw = gfx_->textWidth(tempStr.c_str(), FontBig);
    String rhStr = num0(rh) + "%";
    gfx_->text(x0 + tw + 26, y + 10, rhStr.c_str(), FontBig);

    y += 86;
    if (w.valid) {
        String minmax = num0(w.tempMax) + "\xC2\xB0 | " + num0(w.tempMin) +
                        "\xC2\xB0   percepita " + num0(w.feelsLike) +
                        "\xC2\xB0";
        gfx_->text(x0, y, minmax.c_str(), FontMed);
        y += 30;
        gfx_->text(x0, y, w.description, FontBold);
        y += 40;
    } else {
        gfx_->text(x0, y, "Meteo esterno non disponibile", FontMed, kGrayMid);
        y += 70;
    }

    // Riga dati: pressione, pioggia, vento OWM, nuvolosità.
    const float press = (node != nullptr && !isnan(node->p))
                            ? node->p
                            : (w.valid ? static_cast<float>(w.pressure) : NAN);
    String row;
    row += num0(press) + " hPa";
    if (w.valid) {
        row += "    vis " + String(w.visibility / 1000) + " km";
        row += "    nubi " + String(w.clouds) + "%";
        if (!isnan(w.windSpeed)) {
            row += "    vento " + num1(w.windSpeed) + " m/s";
        }
    }
    gfx_->text(x0, y, row.c_str(), FontMed);

    // Icona meteo grande a destra.
    if (w.valid) {
        icons::weather(*gfx_, kW - 130, kMainTop + 90, 190, w.icon);
    }
}

// ---------------------------------------------------------------------------
// Previsioni (7 slot da 3 ore)
// ---------------------------------------------------------------------------

void DisplayManager::drawForecast() {
    gfx_->hline(0, kForecastTop - 4, kW, kGrayLight);
    const ForecastSlot* fc = weather_->forecast();
    const int colW = kW / static_cast<int>(kForecastSlots);

    for (size_t i = 0; i < kForecastSlots; ++i) {
        const ForecastSlot& s = fc[i];
        const int cx = colW * static_cast<int>(i) + colW / 2;
        if (s.dt == 0) {
            gfx_->text(cx, kForecastTop + 34, "-", FontMed, kGrayLight,
                       TextAlign::Center);
            continue;
        }
        gfx_->text(cx, kForecastTop + 2, time_->timeShortFrom(s.dt).c_str(),
                   FontMed, kInk, TextAlign::Center);
        icons::weather(*gfx_, cx, kForecastTop + 52, 56, s.icon);
        String temps = num0(s.tempMax) + "\xC2\xB0/" + num0(s.tempMin) +
                       "\xC2\xB0";
        gfx_->text(cx, kForecastTop + 78, temps.c_str(), FontSmall, kInk,
                   TextAlign::Center);
        if (s.pop >= 0.05f) {
            String pop = String(static_cast<int>(s.pop * 100)) + "%";
            icons::drop(*gfx_, cx - 24, kForecastTop + 22, 12);
            gfx_->text(cx - 16, kForecastTop + 16, pop.c_str(), FontSmall,
                       kGrayMid);
        }
    }
}

// ---------------------------------------------------------------------------
// Grafici storici
// ---------------------------------------------------------------------------

void DisplayManager::drawGraphs(const NodeInfo* node) {
    gfx_->hline(0, kGraphTop - 4, kW, kGrayLight);
    static const char* kTitles[] = {"Pressione (hPa)", "Temperatura (\xC2\xB0"
                                    "C)",
                                    "Umidita (%)", "Pioggia (mm)"};
    const int chartW = kW / 4;
    float data[kHistorySlots];

    for (int i = 0; i < 4; ++i) {
        // Ordine come nell'immagine: pressione, temperatura, umidità, pioggia
        static const int kSeries[] = {2, 0, 1, 3};
        if (node != nullptr) {
            node->history.series(kSeries[i], data);
        } else {
            for (size_t j = 0; j < kHistorySlots; ++j) {
                data[j] = NAN;
            }
        }
        drawChart(chartW * i + 8, kGraphTop, chartW - 16, kH - kGraphTop - 4,
                  kTitles[i], data, kHistorySlots, i == 3);
    }
}

void DisplayManager::drawChart(int x, int y, int w, int h, const char* title,
                               const float* data, size_t n, bool bars) {
    gfx_->text(x + w / 2, y, title, FontSmall, kInk, TextAlign::Center);
    const int plotY = y + 20;
    const int plotH = h - 40;
    const int labelW = 34;
    const int plotX = x + labelW;
    const int plotW = w - labelW;

    // Range dei dati.
    float mn = NAN;
    float mx = NAN;
    for (size_t i = 0; i < n; ++i) {
        if (isnan(data[i])) {
            continue;
        }
        if (isnan(mn) || data[i] < mn) mn = data[i];
        if (isnan(mx) || data[i] > mx) mx = data[i];
    }
    const bool hasData = !isnan(mn);
    if (!hasData) {
        mn = 0;
        mx = 1;
    }
    if (bars && mn > 0) {
        mn = 0;  // le barre della pioggia partono da zero
    }
    if (mx - mn < 0.5f) {
        const float mid = (mx + mn) / 2;
        mn = mid - 0.5f;
        mx = mid + 0.5f;
    }
    // Margine del 5%.
    const float pad = (mx - mn) * 0.05f;
    mn -= pad;
    mx += pad;

    // Cornice e griglia.
    gfx_->rect(plotX, plotY, plotW, plotH, kGrayMid);
    for (int i = 1; i < 3; ++i) {
        const int gy = plotY + plotH * i / 3;
        for (int gx = plotX; gx < plotX + plotW; gx += 6) {
            gfx_->pixel(gx, gy, kGrayLight);
        }
    }

    // Etichette Y (min e max).
    gfx_->text(plotX - 4, plotY - 6, String(mx, mx - mn > 20 ? 0 : 1).c_str(),
               FontSmall, kGrayMid, TextAlign::Right);
    gfx_->text(plotX - 4, plotY + plotH - 12,
               String(mn, mx - mn > 20 ? 0 : 1).c_str(), FontSmall, kGrayMid,
               TextAlign::Right);
    // Etichette X.
    gfx_->text(plotX, plotY + plotH + 2, "-48h", FontSmall, kGrayMid);
    gfx_->text(plotX + plotW, plotY + plotH + 2, "ora", FontSmall, kGrayMid,
               TextAlign::Right);

    if (!hasData) {
        gfx_->text(plotX + plotW / 2, plotY + plotH / 2 - 8, "nessun dato",
                   FontSmall, kGrayLight, TextAlign::Center);
        return;
    }

    auto toY = [&](float v) {
        return plotY + plotH -
               static_cast<int>((v - mn) / (mx - mn) * (plotH - 2)) - 1;
    };

    if (bars) {
        const float barW = static_cast<float>(plotW) / n;
        for (size_t i = 0; i < n; ++i) {
            if (isnan(data[i]) || data[i] <= 0) {
                continue;
            }
            const int bx = plotX + static_cast<int>(i * barW);
            const int by = toY(data[i]);
            const int bw = barW < 1 ? 1 : static_cast<int>(barW);
            gfx_->fillRect(bx, by, bw, plotY + plotH - by - 1, kInk);
        }
    } else {
        int prevX = -1;
        int prevY = -1;
        for (size_t i = 0; i < n; ++i) {
            if (isnan(data[i])) {
                prevX = -1;
                continue;
            }
            const int px =
                plotX + 1 + static_cast<int>(i * (plotW - 2) / (n - 1));
            const int py = toY(data[i]);
            if (prevX >= 0) {
                gfx_->lineThick(prevX, prevY, px, py, 2, kInk);
            }
            prevX = px;
            prevY = py;
        }
    }
}

// ---------------------------------------------------------------------------
// Schermata di configurazione
// ---------------------------------------------------------------------------

void DisplayManager::showSetupScreen(const String& apSsid, const String& ip) {
    if (!ready_) {
        return;
    }
    gfx_->clear();
    gfx_->text(kW / 2, 90, "Gateway Meteo", FontBig, kInk, TextAlign::Center);
    gfx_->text(kW / 2, 200, "Prima configurazione", FontBold, kInk,
               TextAlign::Center);
    gfx_->text(kW / 2, 270, ("1. Collegati alla rete Wi-Fi:  " + apSsid).c_str(),
               FontMed, kInk, TextAlign::Center);
    gfx_->text(kW / 2, 310,
               ("2. Apri il browser su:  http://" + ip).c_str(), FontMed,
               kInk, TextAlign::Center);
    gfx_->text(kW / 2, 350, "3. Inserisci Wi-Fi, MQTT, LoRa e salva",
               FontMed, kInk, TextAlign::Center);
    gfx_->rect(kW / 2 - 340, 250, 680, 140, kInk);
    flush();
}
