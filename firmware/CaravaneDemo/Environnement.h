// ==== Caravane =======================================================================
// Environnement.h — firmware CaravaneDemo, version 0.10.0 — dernière modification : 2026-11-17
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Environnement — capteurs de la carte CJMCU-8128 en I2C :
//   HDC1080 (0x40) : température et humidité
//   BMP280  (0x76) : pression
//   CCS811  (0x5A ou 0x5B) : qualité d'air (eCO2, COV) — utilisé sur secteur ou en mode Wi-Fi
// Bus : Wire1 (second bus) si les broches diffèrent de celles de l'écran, sinon Wire partagé.
// Pilotes minimalistes écrits ici, sans bibliothèque externe.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Etalonnage.h"

struct Mesures {
  bool  hdcOk = false, bmpOk = false, ccsOk = false;
  float temperatureC = NAN;   // HDC1080
  float humiditePct  = NAN;   // HDC1080
  float pressionHpa  = NAN;   // BMP280
  uint16_t eco2Ppm   = 0;     // CCS811 (0 = pas de valeur)
  uint16_t tvocPpb   = 0;     // CCS811
};

class Environnement {
public:
  // À appeler après la mise sous tension de Ve/Vext et après l'écran.
  // partage = mêmes broches que l'écran : on réutilise Wire, déjà démarré par l'écran.
  void demarrer(uint8_t sda, uint8_t scl, bool partage);
  void balayer();                           // liste les adresses I2C sur le port série
  void lire();                              // HDC1080 + BMP280 (+ CCS811 s'il tourne)
  void definirEtalonnage(const Etalonnage* e) { _etal = e; }   // appliqué à chaque lecture

  // Qualité d'air : démarrée quand le capteur peut rester chaud (secteur ou Wi-Fi).
  void activerQualiteAir(bool actif);
  bool qualiteAirActive() const { return _ccsActif; }

  const Mesures& mesures() const { return _m; }
  const char* nomTH() const;          // "HDC1080", "Si7021" ou "absent"
  String diagnostic() const;          // une ligne lisible pour le moniteur série
  uint8_t adresses(const uint8_t*& liste) const { liste = _adresses; return _nbAdresses; }

private:
  bool ecrire(uint8_t adr, uint8_t reg, const uint8_t* data, size_t n);
  bool lireRegs(uint8_t adr, uint8_t reg, uint8_t* data, size_t n);
  bool present(uint8_t adr);

  bool initBmp();
  void lireHdc();
  void lireSi7021();
  void lireBmp();
  void lireCcs();

  TwoWire* _bus = &Wire1;
  Mesures _m;      // valeurs étalonnées
  Mesures _brut;   // valeurs brutes des capteurs
  uint8_t _adrCcs = 0;
  bool _ccsActif = false;
  const Etalonnage* _etal = nullptr;
  // Température/humidité à 0x40 : HDC1080 (prévu sur la CJMCU-8128) ou Si7021/HTU21D (même adresse)
  enum class TypeTH : uint8_t { Aucun, Hdc1080, Si7021 } _typeTH = TypeTH::Aucun;
  uint8_t _idTH[4] = {};             // identifiants lus au démarrage (diagnostic)
  uint8_t _brutTH[4] = {};           // dernières valeurs brutes (diagnostic)
  uint8_t _adresses[16] = {};        // adresses vues lors du balayage
  uint8_t _nbAdresses = 0;

  // Coefficients d'étalonnage du BMP280
  uint16_t _T1 = 0, _P1 = 0;
  int16_t  _T2 = 0, _T3 = 0, _P2 = 0, _P3 = 0, _P4 = 0, _P5 = 0, _P6 = 0, _P7 = 0, _P8 = 0, _P9 = 0;
};
