/**
 * @file TimeManager.cpp
 * @brief Implementazione della gestione oraria.
 */
#include "TimeManager.h"

#include <Logger.h>

#include <cmath>

namespace {
constexpr const char* kTag = "Time";
constexpr time_t kMinValidEpoch = 1700000000;  // ~nov 2023

struct TzEntry {
    const char* iana;
    const char* posix;
};

/** Tabella dei fusi più comuni; per altri si può inserire direttamente
 *  la stringa POSIX nel campo timezone della configurazione. */
constexpr TzEntry kTzTable[] = {
    {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0"},
    {"Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"UTC", "UTC0"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
};

constexpr const char* kDays[] = {"Dom", "Lun", "Mar", "Mer",
                                 "Gio", "Ven", "Sab"};
constexpr const char* kMonths[] = {"Gen", "Feb", "Mar", "Apr", "Mag", "Giu",
                                   "Lug", "Ago", "Set", "Ott", "Nov", "Dic"};
}  // namespace

void TimeManager::begin(const String& timezone) {
    posixTz_ = toPosixTz(timezone);
    setenv("TZ", posixTz_.c_str(), 1);
    tzset();
}

void TimeManager::startSync() {
    configTzTime(posixTz_.c_str(), "pool.ntp.org", "time.nist.gov",
                 "time.google.com");
    Logger::info(kTag, "Sincronizzazione NTP avviata (TZ: %s)",
                 posixTz_.c_str());
}

bool TimeManager::isSynced() const { return time(nullptr) > kMinValidEpoch; }

time_t TimeManager::now() const {
    const time_t t = time(nullptr);
    return t > kMinValidEpoch ? t : 0;
}

bool TimeManager::localTime(struct tm& out) const {
    const time_t t = now();
    if (t == 0) {
        return false;
    }
    localtime_r(&t, &out);
    return true;
}

String TimeManager::timeShort() const {
    struct tm tm{};
    if (!localTime(tm)) {
        return "--:--";
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    return buf;
}

String TimeManager::dateitalian() const {
    struct tm tm{};
    if (!localTime(tm)) {
        return "--- -- --- ----";
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%s %02d %s %04d", kDays[tm.tm_wday],
             tm.tm_mday, kMonths[tm.tm_mon], tm.tm_year + 1900);
    return buf;
}

String TimeManager::timeShortFrom(time_t t) const {
    if (t <= 0) {
        return "--:--";
    }
    struct tm tm{};
    localtime_r(&t, &tm);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    return buf;
}

float TimeManager::moonPhase(time_t t) {
    // Riferimento: luna nuova del 6 gennaio 2000, 18:14 UTC.
    constexpr double kRefNewMoon = 947182440.0;
    constexpr double kSynodicMonth = 29.530588853 * 86400.0;
    double phase = fmod(static_cast<double>(t) - kRefNewMoon, kSynodicMonth) /
                   kSynodicMonth;
    if (phase < 0) {
        phase += 1.0;
    }
    return static_cast<float>(phase);
}

const char* TimeManager::moonPhaseName(float phase) {
    if (phase < 0.03f || phase > 0.97f) return "Luna nuova";
    if (phase < 0.22f) return "Luna crescente";
    if (phase < 0.28f) return "Primo quarto";
    if (phase < 0.47f) return "Gibbosa crescente";
    if (phase < 0.53f) return "Luna piena";
    if (phase < 0.72f) return "Gibbosa calante";
    if (phase < 0.78f) return "Ultimo quarto";
    return "Luna calante";
}

String TimeManager::toPosixTz(const String& timezone) {
    for (const TzEntry& e : kTzTable) {
        if (timezone.equalsIgnoreCase(e.iana)) {
            return e.posix;
        }
    }
    // Se contiene una virgola o una cifra è verosimilmente già POSIX.
    if (timezone.indexOf(',') >= 0 || timezone.indexOf('0') >= 0 ||
        timezone.indexOf('1') >= 0) {
        return timezone;
    }
    Logger::warn(kTag, "Timezone \"%s\" sconosciuta, uso Europe/Rome",
                 timezone.c_str());
    return kTzTable[0].posix;
}
