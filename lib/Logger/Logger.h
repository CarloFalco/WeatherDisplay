/**
 * @file Logger.h
 * @brief Logging centralizzato con livelli e sink multipli.
 *
 * Sink disponibili: seriale (sempre disponibile), MQTT (tramite callback
 * registrata da MqttManager) e file su LittleFS (opzionale, con rotazione).
 *
 * Nota di design: il logger è volutamente un'unica istanza statica —
 * l'alternativa (iniettarlo in ogni modulo) renderebbe le firme più rumorose
 * senza benefici pratici su questo firmware.
 */
#pragma once

#include <Arduino.h>

#include <functional>

/**
 * @brief Livelli di log in ordine di severità decrescente.
 */
enum class LogLevel : uint8_t { Error = 0, Warning, Info, Debug };

class Logger {
   public:
    /** @brief Callback verso un sink esterno (es. pubblicazione MQTT). */
    using SinkFn = std::function<void(LogLevel, const char* message)>;

    /**
     * @brief Inizializza il logger.
     * @param level  Livello massimo emesso.
     * @param serial true per stampare sulla porta seriale.
     */
    static void begin(LogLevel level, bool serial = true);

    static void setLevel(LogLevel level);
    static LogLevel level();

    /** @brief Converte una stringa ("error".."debug") in LogLevel. */
    static LogLevel levelFromString(const char* s);
    static const char* levelToString(LogLevel level);

    /** @brief Registra il sink esterno (nullptr per rimuoverlo). */
    static void setRemoteSink(SinkFn sink);

    /** @brief Abilita il salvataggio su LittleFS (/log.txt, con rotazione). */
    static void setFileLogging(bool enabled);

    static void error(const char* tag, const char* fmt, ...)
        __attribute__((format(printf, 2, 3)));
    static void warn(const char* tag, const char* fmt, ...)
        __attribute__((format(printf, 2, 3)));
    static void info(const char* tag, const char* fmt, ...)
        __attribute__((format(printf, 2, 3)));
    static void debug(const char* tag, const char* fmt, ...)
        __attribute__((format(printf, 2, 3)));

   private:
    static void log(LogLevel lvl, const char* tag, const char* fmt,
                    va_list args);
    static void writeToFile(const char* line);

    static LogLevel level_;
    static bool serial_;
    static bool file_;
    static SinkFn remoteSink_;
};
