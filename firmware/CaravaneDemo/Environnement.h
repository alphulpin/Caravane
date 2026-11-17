// Environnement — capteurs de la carte CJMCU-8128 en I2C :
//   HDC1080 (0x40) : température et humidité
//   BMP280  (0x76) : pression
//   CCS811  (0x5A ou 0x5B) : qualité d'air (eCO2, COV) — utilisé sur secteur ou en mode Wi-Fi
// Bus : Wire1 (second bus) si les broches diffèrent de celles de l'écran, sinon Wire partagé.
// Pilotes minimalistes écrits ici, sans bibliothèque externe.
#pragma once
#include <Arduino.h>
#include <Wire.h>

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

  // Qualité d'air : démarrée quand le capteur peut rester chaud (secteur ou Wi-Fi).
  void activerQualiteAir(bool actif);
  bool qualiteAirActive() const { return _ccsActif; }

  const Mesures& mesures() const { return _m; }

private:
  bool ecrire(uint8_t adr, uint8_t reg, const uint8_t* data, size_t n);
  bool lireRegs(uint8_t adr, uint8_t reg, uint8_t* data, size_t n);
  bool present(uint8_t adr);

  bool initBmp();
  void lireHdc();
  void lireBmp();
  void lireCcs();

  TwoWire* _bus = &Wire1;
  Mesures _m;
  uint8_t _adrCcs = 0;
  bool _ccsActif = false;

  // Coefficients d'étalonnage du BMP280
  uint16_t _T1 = 0, _P1 = 0;
  int16_t  _T2 = 0, _T3 = 0, _P2 = 0, _P3 = 0, _P4 = 0, _P5 = 0, _P6 = 0, _P7 = 0, _P8 = 0, _P9 = 0;
};
