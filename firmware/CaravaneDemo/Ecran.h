// Ecran — OLED 128x64 intégré à la Heltec V3 (SSD1306, I2C), via la bibliothèque U8g2.
#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

class Ecran {
public:
  Ecran();
  void demarrer();   // alimente (Vext) et initialise l'écran
  void eteindre();   // coupe l'écran et son alimentation avant la veille
  void afficher(const String& l1, const String& l2, const String& l3, const String& l4, const String& etat);

private:
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C _oled;
};
