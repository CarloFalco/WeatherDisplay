/**
 * @file Gfx.cpp
 * @brief Implementazione del canvas grafico 4bpp.
 */
#include "Gfx.h"

#include <cstdlib>
#include <cstring>

Gfx::Gfx(uint8_t* framebuffer, int width, int height)
    : fb_(framebuffer), width_(width), height_(height) {}

void Gfx::clear(uint8_t gray) {
    const uint8_t nib = gray >> 4;
    memset(fb_, (nib << 4) | nib, static_cast<size_t>(width_) * height_ / 2);
}

void Gfx::pixel(int x, int y, uint8_t gray) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    uint8_t* p = &fb_[(static_cast<size_t>(y) * width_ + x) / 2];
    const uint8_t nib = gray >> 4;
    if (x & 1) {
        *p = (*p & 0x0F) | (nib << 4);
    } else {
        *p = (*p & 0xF0) | nib;
    }
}

void Gfx::hline(int x, int y, int w, uint8_t gray) {
    for (int i = 0; i < w; ++i) {
        pixel(x + i, y, gray);
    }
}

void Gfx::vline(int x, int y, int h, uint8_t gray) {
    for (int i = 0; i < h; ++i) {
        pixel(x, y + i, gray);
    }
}

void Gfx::line(int x0, int y0, int x1, int y1, uint8_t gray) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pixel(x0, y0, gray);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Gfx::lineThick(int x0, int y0, int x1, int y1, int thickness,
                    uint8_t gray) {
    if (thickness <= 1) {
        line(x0, y0, x1, y1, gray);
        return;
    }
    const int half = thickness / 2;
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fillRect(x0 - half, y0 - half, thickness, thickness, gray);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Gfx::rect(int x, int y, int w, int h, uint8_t gray) {
    hline(x, y, w, gray);
    hline(x, y + h - 1, w, gray);
    vline(x, y, h, gray);
    vline(x + w - 1, y, h, gray);
}

void Gfx::fillRect(int x, int y, int w, int h, uint8_t gray) {
    for (int j = 0; j < h; ++j) {
        hline(x, y + j, w, gray);
    }
}

void Gfx::circle(int cx, int cy, int r, uint8_t gray) {
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    do {
        pixel(cx - x, cy + y, gray);
        pixel(cx - y, cy - x, gray);
        pixel(cx + x, cy - y, gray);
        pixel(cx + y, cy + x, gray);
        const int e = err;
        if (e <= y) {
            err += ++y * 2 + 1;
        }
        if (e > x || err > y) {
            err += ++x * 2 + 1;
        }
    } while (x < 0);
}

void Gfx::fillCircle(int cx, int cy, int r, uint8_t gray) {
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= r * r) {
                pixel(cx + dx, cy + dy, gray);
            }
        }
    }
}

void Gfx::ring(int cx, int cy, int r, int thickness, uint8_t gray) {
    const int rOut2 = r * r;
    const int rIn = r - thickness;
    const int rIn2 = rIn > 0 ? rIn * rIn : 0;
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            const int d2 = dx * dx + dy * dy;
            if (d2 <= rOut2 && d2 >= rIn2) {
                pixel(cx + dx, cy + dy, gray);
            }
        }
    }
}

void Gfx::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2,
                       uint8_t gray) {
    // Ordina i vertici per Y crescente.
    if (y0 > y1) {
        int t = y0; y0 = y1; y1 = t;
        t = x0; x0 = x1; x1 = t;
    }
    if (y1 > y2) {
        int t = y1; y1 = y2; y2 = t;
        t = x1; x1 = x2; x2 = t;
    }
    if (y0 > y1) {
        int t = y0; y0 = y1; y1 = t;
        t = x0; x0 = x1; x1 = t;
    }
    if (y0 == y2) {
        int minX = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
        int maxX = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
        hline(minX, y0, maxX - minX + 1, gray);
        return;
    }
    // Interpola i bordi riga per riga.
    for (int y = y0; y <= y2; ++y) {
        // Bordo lungo (x0,y0)-(x2,y2)
        const int xa = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        int xb;
        if (y < y1 && y1 != y0) {
            xb = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        } else if (y2 != y1) {
            xb = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        } else {
            xb = x1;
        }
        if (xa <= xb) {
            hline(xa, y, xb - xa + 1, gray);
        } else {
            hline(xb, y, xa - xb + 1, gray);
        }
    }
}

uint32_t Gfx::nextCodepoint(const char** ptr) {
    const uint8_t* s = reinterpret_cast<const uint8_t*>(*ptr);
    uint32_t cp = *s;
    if (cp == 0) {
        return 0;
    }
    int len = 1;
    if ((cp & 0x80) == 0) {
        // ASCII
    } else if ((cp & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((cp & 0x1F) << 6) | (s[1] & 0x3F);
        len = 2;
    } else if ((cp & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 &&
               (s[2] & 0xC0) == 0x80) {
        cp = ((cp & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        len = 3;
    } else {
        cp = '?';
    }
    *ptr += len;
    return cp;
}

const Glyph* Gfx::findGlyph(const BitmapFont& font, uint32_t code) {
    int lo = 0;
    int hi = font.glyphCount - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const uint32_t c = font.glyphs[mid].code;
        if (c == code) {
            return &font.glyphs[mid];
        }
        if (c < code) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return nullptr;
}

int Gfx::textWidth(const char* text, const BitmapFont& font) const {
    int w = 0;
    const char* p = text;
    uint32_t cp;
    while ((cp = nextCodepoint(&p)) != 0) {
        const Glyph* g = findGlyph(font, cp);
        if (g == nullptr) {
            g = findGlyph(font, '?');
        }
        if (g != nullptr) {
            w += g->advance;
        }
    }
    return w;
}

void Gfx::drawGlyph(int x, int y, const Glyph& g, const BitmapFont& font,
                    uint8_t ink) {
    const uint8_t* data = &font.bitmap[g.offset];
    const int bytesPerRow = (g.width + 1) / 2;
    for (int row = 0; row < g.height; ++row) {
        for (int col = 0; col < g.width; ++col) {
            const uint8_t b = data[row * bytesPerRow + col / 2];
            const uint8_t a = (col & 1) ? (b & 0x0F) : (b >> 4);
            if (a == 0) {
                continue;
            }
            // Miscela alpha su fondo bianco: a=15 -> colore pieno.
            const uint8_t gray =
                static_cast<uint8_t>(255 - ((255 - ink) * a) / 15);
            pixel(x + g.left + col, y + g.top + row, gray);
        }
    }
}

void Gfx::text(int x, int y, const char* text, const BitmapFont& font,
               uint8_t ink, TextAlign align) {
    if (align == TextAlign::Center) {
        x -= textWidth(text, font) / 2;
    } else if (align == TextAlign::Right) {
        x -= textWidth(text, font);
    }
    const char* p = text;
    uint32_t cp;
    while ((cp = nextCodepoint(&p)) != 0) {
        const Glyph* g = findGlyph(font, cp);
        if (g == nullptr) {
            g = findGlyph(font, '?');
        }
        if (g == nullptr) {
            continue;
        }
        if (g->width > 0) {
            drawGlyph(x, y, *g, font, ink);
        }
        x += g->advance;
    }
}
