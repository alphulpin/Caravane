// Batterie — tension du connecteur LiPo de la Heltec V3, via le pont diviseur intégré
// (390 kΩ / 100 kΩ, rapport 4,9) lu sur GPIO1 et mis en service par GPIO37 (ADC_Ctrl).
//
// Attention : c'est la tension de la petite batterie LiPo branchée sur la carte,
// pas celle de la batterie 12 V de la caravane (il faudra un pont diviseur externe),
// et pas celle de l'USB. Sans LiPo branchée, la valeur lue n'a pas de sens.
#pragma once
#include <Arduino.h>

class Batterie {
public:
  static constexpr uint8_t BROCHE_MESURE = 1;    // VBAT_Read
  static constexpr uint8_t BROCHE_CTRL   = 37;   // ADC_Ctrl
  static constexpr float   RAPPORT       = 4.9f; // (390k + 100k) / 100k

  // Les V3 et V3.1 ont un tirage au plus sur ADC_Ctrl et s'activent à l'état bas ;
  // la V3.2 a un transistor et s'active à l'état haut. On lit la broche pour savoir.
  void demarrer() {
    pinMode(BROCHE_CTRL, INPUT);
    delay(2);
    _niveauActif = digitalRead(BROCHE_CTRL) == HIGH ? LOW : HIGH;
    pinMode(BROCHE_CTRL, OUTPUT);
    digitalWrite(BROCHE_CTRL, !_niveauActif);   // diviseur coupé hors mesure
    analogSetPinAttenuation(BROCHE_MESURE, ADC_11db);
  }

  // Tension en millivolts (moyenne de 8 lectures calibrées).
  uint16_t lireMv() {
    digitalWrite(BROCHE_CTRL, _niveauActif);
    delay(10);
    uint32_t somme = 0;
    for (uint8_t i = 0; i < 8; i++) somme += analogReadMilliVolts(BROCHE_MESURE);
    digitalWrite(BROCHE_CTRL, !_niveauActif);
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
