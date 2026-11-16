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
  static time_t epoch()     { return time(nullptr); }

  // "16/11/2026 08:10:42" en heure locale, ou "n/d" si l'heure n'a jamais été réglée.
  static String texte(int16_t fuseauMin) {
    if (!valide()) return "n/d";
    time_t local = time(nullptr) + (time_t)fuseauMin * 60;
    struct tm t;
    gmtime_r(&local, &t);
    char tampon[24];
    strftime(tampon, sizeof(tampon), "%d/%m/%Y %H:%M:%S", &t);
    return tampon;
  }
};
