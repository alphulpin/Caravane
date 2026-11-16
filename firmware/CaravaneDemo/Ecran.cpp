#include "Ecran.h"
#include "Config.h"

static const uint8_t CARACTERES_PAR_LIGNE = 21;   // police 6 px sur 128 px

Ecran::Ecran() : _oled(U8G2_R0, BROCHE_OLED_RST, BROCHE_OLED_SCL, BROCHE_OLED_SDA) {}

void Ecran::demarrer() {
  pinMode(BROCHE_VEXT, OUTPUT);
  digitalWrite(BROCHE_VEXT, LOW);    // Vext actif à l'état bas
  delay(50);
  _oled.begin();
  _oled.enableUTF8Print();           // pour les accents
  _oled.setFont(u8g2_font_6x12_tf);
}

void Ecran::eteindre() {
  _oled.setPowerSave(1);
  digitalWrite(BROCHE_VEXT, HIGH);
}

void Ecran::afficher(const String& l1, const String& l2, const String& l3, const String& l4,
                     const String& etat) {
  _oled.clearBuffer();
  const String* lignes[4] = { &l1, &l2, &l3, &l4 };
  for (uint8_t i = 0; i < 4; i++) {
    _oled.setCursor(0, 10 + i * 11);
    _oled.print(lignes[i]->substring(0, CARACTERES_PAR_LIGNE));
  }
  _oled.drawHLine(0, 51, 128);
  _oled.setCursor(0, 63);
  _oled.print(etat);
  _oled.sendBuffer();
}
