// ==== Caravane =======================================================================
// Batterie.h — firmware CaravaneDemo, version 0.9.2 — dernière modification : 2026-11-17
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Batterie — tension du connecteur LiPo de la carte.
// V3 : pont diviseur intégré (390 kΩ / 100 kΩ, rapport 4,9) lu sur GPIO1, mis en service par GPIO37.
// V2 : non géré (BROCHE_VBAT = -1 dans Config.h) : la mesure renvoie 0.
//
// Attention : c'est la tension de la petite batterie LiPo branchée sur la carte,
// pas celle de la batterie 12 V de la caravane (il faudra un pont diviseur externe),
// et pas celle de l'USB. Sans LiPo branchée, la valeur lue n'a pas de sens.
#pragma once
#include <Arduino.h>
#include "Config.h"

class Batterie {
public:
  static constexpr float RAPPORT = 4.9f; // (390k + 100k) / 100k

  static constexpr bool disponible() { return BROCHE_VBAT >= 0; }

  // V3 et V3.1 : tirage au plus sur ADC_Ctrl, actif à l'état bas ;
  // V3.2 : transistor, actif à l'état haut. On lit la broche pour savoir.
  void demarrer() {
    if (!disponible()) return;
    pinMode(BROCHE_VBAT_CTRL, INPUT);
    delay(2);
    _niveauActif = digitalRead(BROCHE_VBAT_CTRL) == HIGH ? LOW : HIGH;
    pinMode(BROCHE_VBAT_CTRL, OUTPUT);
    digitalWrite(BROCHE_VBAT_CTRL, !_niveauActif);   // diviseur coupé hors mesure
    analogSetPinAttenuation(BROCHE_VBAT, ADC_11db);
  }

  // Tension en millivolts (moyenne de 8 lectures calibrées), 0 si non disponible.
  uint16_t lireMv() {
    if (!disponible()) return 0;
    digitalWrite(BROCHE_VBAT_CTRL, _niveauActif);
    delay(10);
    uint32_t somme = 0;
    for (uint8_t i = 0; i < 8; i++) somme += analogReadMilliVolts(BROCHE_VBAT);
    digitalWrite(BROCHE_VBAT_CTRL, !_niveauActif);
    return (uint16_t)((somme / 8) * RAPPORT);
  }

  // Texte court pour l'écran : "4.12V", ou "--" si aucune LiPo plausible.
  static String texte(uint16_t mv) {
    if (mv < 2500 || mv > 4500) return "--";
    char t[8];
    snprintf(t, sizeof(t), "%u.%02uV", mv / 1000, (mv % 1000) / 10);
    return t;
  }

private:
  uint8_t _niveauActif = HIGH;
};
