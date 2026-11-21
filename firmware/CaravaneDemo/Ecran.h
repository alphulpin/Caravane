// ==== Caravane =======================================================================
// Ecran.h — firmware CaravaneDemo, version 0.10.3 — dernière modification : 2026-11-17
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Ecran — OLED 128x64 intégré aux Heltec V2 et V3 (SSD1306, I2C), via la bibliothèque U8g2.
#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

class Ecran {
public:
  Ecran();
  // Alimente Vext et initialise l'écran. busPartage = des capteurs utilisent le même bus I2C
  // (cas de la carte de prototypage V2) : on reste alors à 100 kHz.
  void demarrer(bool busPartage);
  void eteindre();               // coupe l'écran ET Vext (donc les capteurs) avant la veille profonde
  void sommeil(bool actif);      // éteint seulement l'affichage, Vext reste alimenté
  bool endormi() const { return _endormi; }
  void afficher(const String& l1, const String& l2, const String& l3, const String& l4, const String& etat);

private:
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C _oled;
  bool _endormi = false;
  bool _demarre = false;
};
