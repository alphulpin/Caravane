// ==== Caravane =======================================================================
// Ecran.cpp — firmware CaravaneDemo, version 0.10.2 — dernière modification : 2026-11-17
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
#include "Ecran.h"
#include "Config.h"

static const uint8_t CARACTERES_PAR_LIGNE = 21;   // police 6 px sur 128 px

Ecran::Ecran() : _oled(U8G2_R0, BROCHE_OLED_RST, BROCHE_OLED_SCL, BROCHE_OLED_SDA) {}

void Ecran::demarrer(bool busPartage) {
  pinMode(BROCHE_VEXT, OUTPUT);
  digitalWrite(BROCHE_VEXT, LOW);    // Vext actif à l'état bas
  delay(50);
  if (busPartage) _oled.setBusClock(100000);   // le CCS811 étire l'horloge : 100 kHz
  _oled.begin();
  _oled.enableUTF8Print();           // pour les accents
  _oled.setFont(u8g2_font_6x12_tf);
  _endormi = false;
  _demarre = true;
}

void Ecran::eteindre() {
  if (_demarre) _oled.setPowerSave(1);   // pas d'écran démarré lors d'un réveil rapide
  pinMode(BROCHE_VEXT, OUTPUT);
  digitalWrite(BROCHE_VEXT, HIGH);
}

void Ecran::sommeil(bool actif) {
  if (!_demarre || actif == _endormi) return;
  _oled.setPowerSave(actif ? 1 : 0);
  _endormi = actif;
}

void Ecran::afficher(const String& l1, const String& l2, const String& l3, const String& l4,
                     const String& etat) {
  if (!_demarre || _endormi) return;
  _oled.clearBuffer();
  const String* lignes[4] = { &l1, &l2, &l3, &l4 };
  for (uint8_t i = 0; i < 4; i++) {
    _oled.setCursor(0, 10 + i * 11);
    _oled.print(lignes[i]->substring(0, CARACTERES_PAR_LIGNE));
  }
  _oled.drawHLine(0, 51, 128);
  _oled.setCursor(0, 63);
  _oled.print(etat.substring(0, CARACTERES_PAR_LIGNE));
  _oled.sendBuffer();
}
