/**
 * @file Gfx.h
 * @brief Canvas grafico su framebuffer 4bpp per display e-paper.
 *
 * Fornisce primitive di disegno (linee, rettangoli, cerchi) e rendering di
 * testo con font bitmap anti-aliasati generati da tools/gen_font.py.
 * Scala di grigi: 0 = nero, 255 = bianco.
 */
#pragma once

#include <cstdint>

#include "BitmapFont.h"

/**
 * @brief Allineamento orizzontale del testo.
 */
enum class TextAlign : uint8_t { Left, Center, Right };

/**
 * @brief Canvas di disegno su framebuffer 4bpp (2 pixel per byte).
 */
class Gfx {
   public:
    /**
     * @param framebuffer Buffer 4bpp di dimensione width*height/2.
     * @param width       Larghezza in pixel (pari).
     * @param height      Altezza in pixel.
     */
    Gfx(uint8_t* framebuffer, int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    uint8_t* buffer() { return fb_; }

    /** @brief Riempie l'intero canvas con un livello di grigio. */
    void clear(uint8_t gray = 255);

    /** @brief Disegna un singolo pixel (no-op fuori dai bordi). */
    void pixel(int x, int y, uint8_t gray);

    void hline(int x, int y, int w, uint8_t gray);
    void vline(int x, int y, int h, uint8_t gray);

    /** @brief Linea generica (Bresenham). */
    void line(int x0, int y0, int x1, int y1, uint8_t gray);

    /** @brief Linea con spessore (disegnata come quadrati lungo il percorso). */
    void lineThick(int x0, int y0, int x1, int y1, int thickness, uint8_t gray);

    void rect(int x, int y, int w, int h, uint8_t gray);
    void fillRect(int x, int y, int w, int h, uint8_t gray);

    void circle(int cx, int cy, int r, uint8_t gray);
    void fillCircle(int cx, int cy, int r, uint8_t gray);

    /** @brief Corona circolare (cerchio con spessore). */
    void ring(int cx, int cy, int r, int thickness, uint8_t gray);

    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2,
                      uint8_t gray);

    /**
     * @brief Larghezza in pixel di una stringa UTF-8 renderizzata.
     */
    int textWidth(const char* text, const BitmapFont& font) const;

    /**
     * @brief Disegna testo UTF-8.
     *
     * @param x    Coordinata X (interpretata secondo @p align).
     * @param y    Coordinata Y del top della riga di testo.
     * @param ink  Livello di grigio dell'inchiostro (0 = nero).
     */
    void text(int x, int y, const char* text, const BitmapFont& font,
              uint8_t ink = 0, TextAlign align = TextAlign::Left);

   private:
    /** @brief Decodifica il prossimo codepoint UTF-8 (avanza il puntatore). */
    static uint32_t nextCodepoint(const char** ptr);

    /** @brief Ricerca binaria del glifo; nullptr se assente. */
    static const Glyph* findGlyph(const BitmapFont& font, uint32_t code);

    void drawGlyph(int x, int y, const Glyph& g, const BitmapFont& font,
                   uint8_t ink);

    uint8_t* fb_;
    int width_;
    int height_;
};
