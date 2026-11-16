// Reglages — paramètres modifiables par le téléphone, conservés en mémoire flash (NVS).
#pragma once
#include <Arduino.h>
#include <Preferences.h>

class Reglages {
public:
  void charger();                       // lit la flash, sinon valeurs par défaut de Config.h
  void reinitialiser();                 // retour aux valeurs codées en dur

  // Chaque "definir" ignore une valeur vide/nulle et renvoie true si le réglage a changé.
  bool definirMessage(const String& texte);
  bool definirInactivite(uint32_t secondes);
  bool definirVeille(uint32_t secondes);
  bool definirFuseau(int16_t minutes);

  const String& message() const { return _message; }
  uint32_t inactiviteS() const  { return _inactiviteS; }
  uint32_t veilleS() const      { return _veilleS; }
  int16_t fuseauMin() const     { return _fuseauMin; }

private:
  Preferences _nvs;
  String   _message;
  uint32_t _inactiviteS = 0;
  uint32_t _veilleS = 0;
  int16_t  _fuseauMin = 0;
};
