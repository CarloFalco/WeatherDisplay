/**
 * @file Icons.h
 * @brief Icone disegnate proceduralmente per la dashboard e-paper.
 */
#pragma once

#include "Gfx.h"

namespace icons {

/**
 * @brief Disegna l'icona meteo corrispondente a un codice OpenWeatherMap.
 *
 * Codici supportati: 01d/01n (sereno), 02x (poco nuvoloso), 03x (nubi),
 * 04x (coperto), 09x (rovesci), 10x (pioggia), 11x (temporale),
 * 13x (neve), 50x (nebbia).
 *
 * @param cx   Centro X.
 * @param cy   Centro Y.
 * @param size Dimensione indicativa (larghezza) dell'icona in pixel.
 * @param code Codice icona OWM (es. "10d"); nullptr = icona "n/d".
 */
void weather(Gfx& g, int cx, int cy, int size, const char* code);

/** @brief Icona Wi-Fi ad archi. @p connected false = barrata. */
void wifi(Gfx& g, int x, int y, int size, bool connected);

/** @brief Icona broker MQTT (nodo con frecce). @p connected false = barrata. */
void mqtt(Gfx& g, int x, int y, int size, bool connected);

/** @brief Icona antenna LoRa. @p active false = barrata. */
void lora(Gfx& g, int x, int y, int size, bool active);

/**
 * @brief Icona batteria orizzontale con livello di riempimento.
 * @param pct Percentuale 0-100; valori negativi = sconosciuta (vuota con "?").
 */
void battery(Gfx& g, int x, int y, int w, int h, int pct);

/**
 * @brief Fase lunare come disco parzialmente ombreggiato.
 * @param phase Fase 0..1 (0 = luna nuova, 0.5 = piena).
 */
void moon(Gfx& g, int cx, int cy, int r, float phase);

/** @brief Goccia (umidità). */
void drop(Gfx& g, int cx, int cy, int size);

}  // namespace icons
