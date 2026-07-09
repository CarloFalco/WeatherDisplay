/**
 * @file Icons.cpp
 * @brief Implementazione delle icone procedurali.
 */
#include "Icons.h"

#include <cmath>
#include <cstring>

namespace icons {

namespace {

constexpr uint8_t kInk = 0;
constexpr uint8_t kPaper = 255;
constexpr uint8_t kGray = 128;

/** @brief Nuvola: due cerchi pieni + base, con contorno spesso. */
void cloudShape(Gfx& g, int cx, int cy, int size, uint8_t fill) {
    const int r1 = size / 4;             // gobba sinistra
    const int r2 = size / 3;             // gobba destra (più alta)
    const int baseH = size / 5;
    const int t = size >= 60 ? 4 : 2;    // spessore contorno

    const int x1 = cx - size / 5;
    const int y1 = cy - baseH / 2 - r1 / 2;
    const int x2 = cx + size / 6;
    const int y2 = cy - baseH / 2 - r2 / 2;
    const int bx = cx - size / 2;
    const int by = cy - baseH;
    const int bw = size;
    const int bh = baseH * 2;

    // Contorno (forme leggermente più grandi in inchiostro)
    g.fillCircle(x1, y1, r1 + t, kInk);
    g.fillCircle(x2, y2, r2 + t, kInk);
    g.fillRect(bx - t, by - t, bw + 2 * t, bh + 2 * t, kInk);
    // Interno
    g.fillCircle(x1, y1, r1, fill);
    g.fillCircle(x2, y2, r2, fill);
    g.fillRect(bx, by, bw, bh, fill);
}

/** @brief Sole: disco + raggi. */
void sunShape(Gfx& g, int cx, int cy, int r) {
    const int t = r >= 20 ? 3 : 2;
    g.ring(cx, cy, r, t, kInk);
    for (int i = 0; i < 8; ++i) {
        const float a = i * static_cast<float>(M_PI) / 4.0f;
        const int x0 = cx + static_cast<int>(cosf(a) * (r + r / 3));
        const int y0 = cy + static_cast<int>(sinf(a) * (r + r / 3));
        const int x1 = cx + static_cast<int>(cosf(a) * (r + r * 3 / 4));
        const int y1 = cy + static_cast<int>(sinf(a) * (r + r * 3 / 4));
        g.lineThick(x0, y0, x1, y1, t, kInk);
    }
}

/** @brief Falce di luna. */
void moonShape(Gfx& g, int cx, int cy, int r) {
    g.fillCircle(cx, cy, r, kInk);
    g.fillCircle(cx + r / 2, cy - r / 3, r, kPaper);
}

/** @brief Gocce di pioggia sotto una nuvola. */
void rainDrops(Gfx& g, int cx, int cy, int size, int count) {
    const int t = size >= 60 ? 3 : 2;
    const int span = size * 2 / 3;
    for (int i = 0; i < count; ++i) {
        const int x = cx - span / 2 + (count > 1 ? span * i / (count - 1) : 0);
        g.lineThick(x, cy, x - size / 12, cy + size / 5, t, kInk);
    }
}

/** @brief Fulmine. */
void bolt(Gfx& g, int cx, int cy, int size) {
    const int s = size / 3;
    g.fillTriangle(cx, cy - s / 2, cx - s / 2, cy + s / 3, cx, cy + s / 4,
                   kInk);
    g.fillTriangle(cx, cy - s / 4, cx + s / 2, cy - s / 3, cx, cy + s / 2,
                   kInk);
}

/** @brief Fiocco di neve (asterisco). */
void flake(Gfx& g, int cx, int cy, int size) {
    const int r = size / 6;
    for (int i = 0; i < 3; ++i) {
        const float a = i * static_cast<float>(M_PI) / 3.0f;
        const int dx = static_cast<int>(cosf(a) * r);
        const int dy = static_cast<int>(sinf(a) * r);
        g.line(cx - dx, cy - dy, cx + dx, cy + dy, kInk);
    }
}

}  // namespace

void weather(Gfx& g, int cx, int cy, int size, const char* code) {
    if (code == nullptr || strlen(code) < 2) {
        // Nessun dato: cerchio tratteggiato.
        g.ring(cx, cy, size / 3, 2, kGray);
        return;
    }
    const bool night = code[2] == 'n';
    const char c0 = code[0];
    const char c1 = code[1];

    if (c0 == '0' && c1 == '1') {  // sereno
        if (night) {
            moonShape(g, cx, cy, size / 3);
        } else {
            sunShape(g, cx, cy, size / 3);
        }
    } else if (c0 == '0' && c1 == '2') {  // poco nuvoloso
        if (night) {
            moonShape(g, cx - size / 5, cy - size / 5, size / 4);
        } else {
            sunShape(g, cx - size / 5, cy - size / 5, size / 5);
        }
        cloudShape(g, cx + size / 12, cy + size / 8, size * 2 / 3, kPaper);
    } else if (c0 == '0' && c1 == '3') {  // nubi sparse
        cloudShape(g, cx, cy, size * 3 / 4, kPaper);
    } else if (c0 == '0' && c1 == '4') {  // coperto
        cloudShape(g, cx - size / 8, cy - size / 6, size / 2, kGray);
        cloudShape(g, cx + size / 12, cy + size / 10, size * 3 / 4, kPaper);
    } else if (c0 == '0' && c1 == '9') {  // rovesci
        cloudShape(g, cx, cy - size / 8, size * 2 / 3, kPaper);
        rainDrops(g, cx, cy + size / 4, size, 3);
    } else if (c0 == '1' && c1 == '0') {  // pioggia
        if (!night) {
            sunShape(g, cx - size / 4, cy - size / 4, size / 6);
        }
        cloudShape(g, cx, cy - size / 8, size * 2 / 3, kPaper);
        rainDrops(g, cx, cy + size / 4, size, 2);
    } else if (c0 == '1' && c1 == '1') {  // temporale
        cloudShape(g, cx, cy - size / 8, size * 2 / 3, kGray);
        bolt(g, cx, cy + size / 4, size);
    } else if (c0 == '1' && c1 == '3') {  // neve
        cloudShape(g, cx, cy - size / 8, size * 2 / 3, kPaper);
        flake(g, cx - size / 5, cy + size / 4, size / 2);
        flake(g, cx + size / 6, cy + size / 3, size / 2);
    } else if (c0 == '5' && c1 == '0') {  // nebbia
        const int t = size >= 60 ? 3 : 2;
        for (int i = 0; i < 4; ++i) {
            const int y = cy - size / 4 + i * size / 6;
            g.fillRect(cx - size / 3, y, size * 2 / 3, t, kInk);
        }
    } else {  // sconosciuto
        cloudShape(g, cx, cy, size * 2 / 3, kPaper);
    }
}

void wifi(Gfx& g, int x, int y, int size, bool connected) {
    // Archi concentrici con vertice in basso al centro.
    const int cx = x + size / 2;
    const int cy = y + size - 2;
    g.fillCircle(cx, cy, size / 10 + 1, kInk);
    for (int i = 1; i <= 3; ++i) {
        const int r = size * i / 3;
        // Porzione superiore dell'anello (angolo ~90°)
        for (int dy = -r; dy <= 0; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                const int d2 = dx * dx + dy * dy;
                if (d2 <= r * r && d2 >= (r - 2) * (r - 2) &&
                    abs(dx) <= -dy + r / 4) {
                    g.pixel(cx + dx, cy + dy, kInk);
                }
            }
        }
    }
    if (!connected) {
        g.lineThick(x, y, x + size, y + size, 3, kInk);
    }
}

void mqtt(Gfx& g, int x, int y, int size, bool connected) {
    // Riquadro con lettera "M" stilizzata a linee.
    g.rect(x, y + 2, size, size - 2, kInk);
    g.rect(x + 1, y + 3, size - 2, size - 4, kInk);
    const int mx0 = x + size / 5;
    const int mx1 = x + size - size / 5;
    const int my0 = y + size - size / 4;
    const int my1 = y + size / 3;
    g.line(mx0, my0, mx0, my1, kInk);
    g.line(mx0, my1, x + size / 2, my0 - size / 6, kInk);
    g.line(x + size / 2, my0 - size / 6, mx1, my1, kInk);
    g.line(mx1, my1, mx1, my0, kInk);
    if (!connected) {
        g.lineThick(x, y, x + size, y + size, 3, kInk);
    }
}

void lora(Gfx& g, int x, int y, int size, bool active) {
    // Antenna: asta con triangolo e onde.
    const int cx = x + size / 2;
    const int top = y + 2;
    g.lineThick(cx, top, cx, y + size, 2, kInk);
    g.fillTriangle(cx - size / 4, top, cx + size / 4, top, cx, top + size / 3,
                   kInk);
    // Onde laterali
    for (int i = 1; i <= 2; ++i) {
        const int r = size * i / 4 + size / 8;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = 0; dx <= r; ++dx) {
                const int d2 = dx * dx + dy * dy;
                if (d2 <= r * r && d2 >= (r - 2) * (r - 2) &&
                    abs(dy) < dx) {
                    g.pixel(cx + dx + 2, y + size / 3 + dy, kInk);
                    g.pixel(cx - dx - 2, y + size / 3 + dy, kInk);
                }
            }
        }
    }
    if (!active) {
        g.lineThick(x, y, x + size, y + size, 3, kInk);
    }
}

void battery(Gfx& g, int x, int y, int w, int h, int pct) {
    const int tipW = w / 10 + 1;
    g.rect(x, y, w - tipW, h, kInk);
    g.rect(x + 1, y + 1, w - tipW - 2, h - 2, kInk);
    g.fillRect(x + w - tipW, y + h / 4, tipW, h / 2, kInk);
    if (pct >= 0) {
        const int maxFill = w - tipW - 6;
        int fill = maxFill * (pct > 100 ? 100 : pct) / 100;
        if (fill > 0) {
            g.fillRect(x + 3, y + 3, fill, h - 6, kInk);
        }
    }
}

void moon(Gfx& g, int cx, int cy, int r, float phase) {
    // Disco base grigio chiaro con contorno.
    g.fillCircle(cx, cy, r, 220);
    g.ring(cx, cy, r, 2, kInk);
    // Ombreggia la parte non illuminata riga per riga.
    // phase: 0 = nuova (tutta scura), 0.5 = piena (tutta chiara), 1 = nuova.
    const float ph = phase - floorf(phase);
    for (int dy = -r + 2; dy <= r - 2; ++dy) {
        const float halfW = sqrtf(static_cast<float>(r * r - dy * dy));
        // Terminatore: curva ellittica che attraversa il disco.
        const float k = cosf(2.0f * static_cast<float>(M_PI) * ph);
        const float term = halfW * k;
        int x0, x1;
        if (ph < 0.5f) {
            // Crescente: illumina da destra.
            x0 = static_cast<int>(-halfW);
            x1 = static_cast<int>(term);
        } else {
            // Calante: illumina da sinistra.
            x0 = static_cast<int>(term);
            x1 = static_cast<int>(halfW);
        }
        for (int dx = x0; dx <= x1; ++dx) {
            g.pixel(cx + dx, cy + dy, 64);
        }
    }
    g.ring(cx, cy, r, 2, kInk);
}

void drop(Gfx& g, int cx, int cy, int size) {
    const int r = size / 3;
    g.fillCircle(cx, cy + size / 6, r, kInk);
    g.fillTriangle(cx - r, cy + size / 6, cx + r, cy + size / 6, cx,
                   cy - size / 2, kInk);
}

}  // namespace icons
