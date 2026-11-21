// ==== Caravane =======================================================================
// Reglages.h — firmware CaravaneDemo, version 0.9.3 — dernière modification : 2026-11-19
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Reglages — paramètres modifiables par le téléphone, conservés en mémoire flash (NVS).
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"
#include "Etalonnage.h"

class Reglages {
public:
  void charger();                       // lit la flash, sinon valeurs par défaut de Config.h
  void reinitialiser();                 // temporisations d'origine (garde nom, Wi-Fi, Supabase, broches)

  // Chaque "definir" ignore une valeur vide/nulle et renvoie true si le réglage a changé.
  bool definirNom(const String& nom);
  bool effacerNom();
  bool definirInactivite(uint32_t secondes);
  bool definirVeille(uint32_t secondes);
  bool definirFuseau(int16_t minutes);
  bool definirReleve(uint32_t secondes);
  bool journal() const           { return _journal; }   // journal série (gardé par « valeurs par défaut »)
  bool definirJournal(bool actif);
  bool definirComm(const String& mode);  // "aucun", "wifi", "lora", "wifi_lora"
  bool definirWifi(const String& ssid, const String& mdp);
  bool definirSupabase(const String& url, const String& cle, const String& table);
  // Broches : -1 = défaut de la carte ; valeur absente = inchangée. Prises en compte au redémarrage.
  bool definirBroche(const char* nom, int valeur);

  // Numéro du prochain relevé (croissant, conservé en flash : jamais réutilisé)
  uint32_t prochainNumero();

  const String& nom() const      { return _nom; }        // vide = pas de nom
  String nomAffiche() const      { return _nom.length() ? _nom : String(NOM_DEFAUT); }
  uint32_t inactiviteS() const   { return _inactiviteS; }
  uint32_t veilleS() const       { return _veilleS; }
  int16_t fuseauMin() const      { return _fuseauMin; }
  uint32_t releveS() const       { return _releveS; }
  const Etalonnage& etalonnage() const { return _etal; }
  bool definirEtalon(uint8_t g, float pente, float decalage);   // gardé par « valeurs par défaut »
  ModeComm comm() const          { return _comm; }
  bool wifiActif() const         { return _comm == ModeComm::Wifi || _comm == ModeComm::WifiLora; }
  bool loraActif() const         { return _comm == ModeComm::Lora || _comm == ModeComm::WifiLora; }
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
  String   _nom;
  bool _journal = true;
  uint32_t _inactiviteS = 0, _veilleS = 0, _releveS = 0, _numero = 0;
  int16_t  _fuseauMin = 0;
  Etalonnage _etal;
  ModeComm _comm = ModeComm::Aucun;
  String   _wifiSsid, _wifiMdp, _sbUrl, _sbCle, _sbTable;
  int8_t   _sda = -1, _scl = -1, _secteur = -1;
};
