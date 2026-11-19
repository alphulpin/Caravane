// ==== Caravane =======================================================================
// Horloge.h — firmware CaravaneDemo, version 0.9.0 — dernière modification : 2026-11-17
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Horloge — heure système de l'ESP32 (conservée pendant la veille profonde,
// perdue à la coupure d'alimentation : elle redevient alors "n/d").
#pragma once
#include <Arduino.h>
#include <sys/time.h>

class Horloge {
public:
  // epoch UTC en secondes ; accepte aussi des millisecondes (Date.now() en JS).
  static bool regler(int64_t epoch) {
    if (epoch > 100000000000LL) epoch /= 1000;   // c'était des millisecondes
    if (epoch < 1700000000LL) return false;      // valeur absente ou fantaisiste
    timeval tv = { (time_t)epoch, 0 };
    settimeofday(&tv, nullptr);
    return true;
  }

  static bool valide()      { return time(nullptr) > 1700000000; }

  // "2026-11-17T06:14:00Z" (UTC), pour Supabase
  static String iso8601(time_t instant) {
    struct tm u;
    gmtime_r(&instant, &u);
    char t[48];
    snprintf(t, sizeof(t), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             u.tm_year + 1900, u.tm_mon + 1, u.tm_mday, u.tm_hour, u.tm_min, u.tm_sec);
    return t;
  }
  static time_t epoch()     { return time(nullptr); }

  // "16/11/2026 08:10:42" en heure locale, ou "n/d" si l'heure n'a jamais été réglée.
  static String texte(int16_t fuseauMin) {
    if (!valide()) return "n/d";
    time_t local = time(nullptr) + (time_t)fuseauMin * 60;
    struct tm t;
    gmtime_r(&local, &t);
    // snprintf plutôt que strftime : strftime occupe 3 Ko d'IRAM, qui manquent sur l'ESP32 (V2)
    char tampon[48];
    snprintf(tampon, sizeof(tampon), "%02d/%02d/%04d %02d:%02d:%02d",
             t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec);
    return tampon;
  }
};
