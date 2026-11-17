// Reglages — paramètres modifiables par le téléphone, conservés en mémoire flash (NVS).
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

class Reglages {
public:
  void charger();                       // lit la flash, sinon valeurs par défaut de Config.h
  void reinitialiser();                 // message et temporisations d'origine (garde Wi-Fi, Supabase, broches)

  // Chaque "definir" ignore une valeur vide/nulle et renvoie true si le réglage a changé.
  bool definirMessage(const String& texte);
  bool definirInactivite(uint32_t secondes);
  bool definirVeille(uint32_t secondes);
  bool definirFuseau(int16_t minutes);
  bool definirEnvoi(uint32_t secondes);
  bool definirComm(const String& mode);  // "aucun", "wifi", "lora", "wifi_lora"
  bool definirWifi(const String& ssid, const String& mdp);
  bool definirSupabase(const String& url, const String& cle, const String& table);
  // Broches : -1 = défaut de la carte ; valeur absente = inchangée. Prises en compte au redémarrage.
  bool definirBroche(const char* nom, int valeur);

  const String& message() const { return _message; }
  uint32_t inactiviteS() const  { return _inactiviteS; }
  uint32_t veilleS() const      { return _veilleS; }
  int16_t fuseauMin() const     { return _fuseauMin; }
  uint32_t envoiS() const       { return _envoiS; }
  ModeComm comm() const         { return _comm; }
  bool wifiActif() const        { return _comm == ModeComm::Wifi || _comm == ModeComm::WifiLora; }
  bool loraActif() const        { return _comm == ModeComm::Lora || _comm == ModeComm::WifiLora; }
  const String& wifiSsid() const { return _wifiSsid; }
  const String& wifiMdp() const  { return _wifiMdp; }
  const String& sbUrl() const    { return _sbUrl; }
  const String& sbCle() const    { return _sbCle; }
  const String& sbTable() const  { return _sbTable; }
  bool supabasePret() const      { return _sbUrl.length() && _sbCle.length() && _sbTable.length(); }

  uint8_t capteursSda() const { return _sda < 0 ? DEFAUT_CAPTEURS_SDA : _sda; }
  uint8_t capteursScl() const { return _scl < 0 ? DEFAUT_CAPTEURS_SCL : _scl; }
  uint8_t secteur() const     { return _secteur < 0 ? DEFAUT_SECTEUR : _secteur; }

  static const char* nomComm(ModeComm m);

private:
  Preferences _nvs;
  String   _message;
  uint32_t _inactiviteS = 0, _veilleS = 0, _envoiS = 0;
  int16_t  _fuseauMin = 0;
  ModeComm _comm = ModeComm::Aucun;
  String   _wifiSsid, _wifiMdp, _sbUrl, _sbCle, _sbTable;
  int8_t   _sda = -1, _scl = -1, _secteur = -1;
};
