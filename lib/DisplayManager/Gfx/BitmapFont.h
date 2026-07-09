/**
 * @file BitmapFont.h
 * @brief Strutture dati per i font bitmap 4bpp generati da tools/gen_font.py.
 */
#pragma once

#include <cstdint>

/**
 * @brief Singolo glifo di un font bitmap.
 *
 * Il bitmap è impacchettato a 4 bit per pixel (nibble alto = primo pixel),
 * riga per riga. 0 = trasparente, 15 = inchiostro pieno.
 */
struct Glyph {
    uint32_t code;     ///< Codepoint Unicode.
    uint8_t  width;    ///< Larghezza bitmap in pixel.
    uint8_t  height;   ///< Altezza bitmap in pixel.
    int8_t   left;     ///< Offset orizzontale dal cursore.
    int8_t   top;      ///< Offset verticale dal top della riga.
    uint8_t  advance;  ///< Avanzamento del cursore.
    uint32_t offset;   ///< Offset nel blocco bitmap.
};

/**
 * @brief Font bitmap completo (tabella glifi ordinata per codepoint).
 */
struct BitmapFont {
    const uint8_t* bitmap;      ///< Dati bitmap 4bpp.
    const Glyph*   glyphs;      ///< Tabella glifi, ordinata per codepoint.
    uint16_t       glyphCount;  ///< Numero di glifi.
    int16_t        ascent;      ///< Ascender in pixel.
    int16_t        descent;     ///< Descender in pixel.
    int16_t        lineHeight;  ///< Altezza riga (ascent + descent).
};
