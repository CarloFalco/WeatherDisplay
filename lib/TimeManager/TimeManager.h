/**
 * @file TimeManager.h
 * @brief Sincronizzazione NTP e utilità di data/ora localizzate.
 */
#pragma once

#include <Arduino.h>

#include <ctime>

/**
 * @brief Gestisce la sincronizzazione oraria via NTP e la formattazione
 *        di data/ora nel fuso configurato.
 */
class TimeManager {
   public:
    /**
     * @brief Memorizza il fuso orario (nome IANA, es. "Europe/Rome",
     *        oppure stringa POSIX TZ diretta).
     */
    void begin(const String& timezone);

    /** @brief Avvia la sincronizzazione NTP (da chiamare a Wi-Fi connesso). */
    void startSync();

    /** @brief true se l'orologio di sistema è stato sincronizzato. */
    bool isSynced() const;

    /** @brief Epoch corrente (0 se non sincronizzato). */
    time_t now() const;

    /** @brief Ora locale corrente; false se non sincronizzato. */
    bool localTime(struct tm& out) const;

    /** @brief "HH:MM". */
    String timeShort() const;

    /** @brief Data italiana, es. "Lun 08 Lug 2026". */
    String dateitalian() const;

    /** @brief "HH:MM" da un epoch arbitrario (fuso locale). */
    String timeShortFrom(time_t t) const;

    /**
     * @brief Fase lunare corrente, 0..1 (0 = nuova, 0.5 = piena).
     */
    static float moonPhase(time_t t);

    /** @brief Nome italiano della fase lunare. */
    static const char* moonPhaseName(float phase);

    /**
     * @brief Converte un nome di fuso IANA in stringa POSIX TZ.
     *        Ritorna la stringa stessa se sembra già in formato POSIX.
     */
    static String toPosixTz(const String& timezone);

   private:
    String posixTz_;
};
