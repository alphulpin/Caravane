// ==== Caravane =======================================================================
// Reglages.cpp — firmware CaravaneDemo, version 0.9.3 — dernière modification : 2026-11-19
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
#include "Reglages.h"

static const char* ESPACE_NVS = "caravane";

const char* Reglages::nomComm(ModeComm m) {
  switch (m) {
    case ModeComm::Wifi:     return "wifi";
    case ModeComm::Lora:     return "lora";
    case ModeComm::WifiLora: return "wifi_lora";
    default:                 return "aucun";
  }
}

void Reglages::charger() {
  _nvs.begin(ESPACE_NVS, false);
  _nom         = _nvs.getString("nom", "");
  _inactiviteS = _nvs.getULong("inact", INACTIVITE_DEFAUT_S);
  _veilleS     = _nvs.getULong("veille", VEILLE_DEFAUT_S);
  _fuseauMin   = _nvs.getShort("tz", FUSEAU_DEFAUT_MIN);
  _releveS     = _nvs.getULong("releve", RELEVE_DEFAUT_S);
  _journal     = _nvs.getBool("journal", true);
  if (_nvs.getBytesLength("etal") == Etalonnage::TAILLE) _nvs.getBytes("etal", _etal.donnees(), Etalonnage::TAILLE);
  else if (_nvs.isKey("tcorr")) _etal.definir(G_TEMP, 1.0f, _nvs.getShort("tcorr", 0) / 10.0f);   // ancienne correction
  _numero      = _nvs.getULong("numero", 0);
  _comm        = (ModeComm)_nvs.getUChar("comm", (uint8_t)ModeComm::Aucun);
  _wifiSsid    = _nvs.getString("wssid", "");
  _wifiMdp     = _nvs.getString("wmdp", "");
  _sbUrl       = _nvs.getString("sburl", "");
  _sbCle       = _nvs.getString("sbcle", "");
  _sbTable     = _nvs.getString("sbtab", SB_TABLE_DEFAUT);
  if (_sbTable == "caravane_mesures") {          // ancienne table « une colonne par capteur »
    _sbTable = SB_TABLE_DEFAUT;
    _nvs.putString("sbtab", _sbTable);
  }
  _sda         = _nvs.getChar("bsda", -1);
  _scl         = _nvs.getChar("bscl", -1);
  _secteur     = _nvs.getChar("bsect", -1);
  if (!CARTE_LORA && loraActif()) _comm = wifiActif() ? ModeComm::Wifi : ModeComm::Aucun;
}

void Reglages::reinitialiser() {
  const char* cles[] = { "inact", "veille", "tz", "releve" };
  for (const char* c : cles) _nvs.remove(c);
  _inactiviteS = INACTIVITE_DEFAUT_S;
  _veilleS     = VEILLE_DEFAUT_S;
  _fuseauMin   = FUSEAU_DEFAUT_MIN;
  _releveS     = RELEVE_DEFAUT_S;
}

bool Reglages::definirNom(const String& nom) {
  String t = nom;
  t.trim();
  if (t.length() == 0) return false;                       // champ vide : on ne touche à rien
  if (t.length() > NOM_LONGUEUR_MAX) t = t.substring(0, NOM_LONGUEUR_MAX);
  if (t == _nom) return false;
  _nom = t;
  _nvs.putString("nom", _nom);
  return true;
}

bool Reglages::effacerNom() {
  if (_nom.length() == 0) return false;
  _nom = "";
  _nvs.remove("nom");
  return true;
}

uint32_t Reglages::prochainNumero() {
  _numero++;
  _nvs.putULong("numero", _numero);
  return _numero;
}

bool Reglages::definirInactivite(uint32_t s) {
  if (s == 0) return false;
  s = constrain(s, INACTIVITE_MIN_S, INACTIVITE_MAX_S);
  if (s == _inactiviteS) return false;
  _inactiviteS = s;
  _nvs.putULong("inact", s);
  return true;
}

bool Reglages::definirVeille(uint32_t s) {
  if (s == 0) return false;
  s = constrain(s, VEILLE_MIN_S, VEILLE_MAX_S);
  if (s == _veilleS) return false;
  _veilleS = s;
  _nvs.putULong("veille", s);
  return true;
}

bool Reglages::definirFuseau(int16_t minutes) {
  if (minutes < -720 || minutes > 840 || minutes == _fuseauMin) return false;
  _fuseauMin = minutes;
  _nvs.putShort("tz", minutes);
  return true;
}

bool Reglages::definirEtalon(uint8_t g, float pente, float decalage) {
  if (!_etal.definir(g, pente, decalage)) return false;
  _nvs.putBytes("etal", _etal.donnees(), Etalonnage::TAILLE);
  return true;
}

bool Reglages::definirReleve(uint32_t s) {
  if (s == 0) return false;
  s = constrain(s, RELEVE_MIN_S, RELEVE_MAX_S);
  if (s == _releveS) return false;
  _releveS = s;
  _nvs.putULong("releve", s);
  return true;
}

bool Reglages::definirComm(const String& mode) {
  ModeComm m;
  if (mode == "aucun") m = ModeComm::Aucun;
  else if (mode == "wifi") m = ModeComm::Wifi;
  else if (mode == "lora") m = ModeComm::Lora;
  else if (mode == "wifi_lora") m = ModeComm::WifiLora;
  else return false;
  if (!CARTE_LORA && (m == ModeComm::Lora || m == ModeComm::WifiLora)) return false;
  if (m == _comm) return false;
  _comm = m;
  _nvs.putUChar("comm", (uint8_t)m);
  return true;
}

bool Reglages::definirWifi(const String& ssid, const String& mdp) {
  bool change = false;
  if (ssid.length() && ssid != _wifiSsid) { _wifiSsid = ssid; _nvs.putString("wssid", ssid); change = true; }
  if (mdp.length() && mdp != _wifiMdp)    { _wifiMdp = mdp;   _nvs.putString("wmdp", mdp);   change = true; }
  return change;
}

bool Reglages::definirSupabase(const String& url, const String& cle, const String& table) {
  bool change = false;
  String u = url;
  u.trim();
  while (u.endsWith("/")) u.remove(u.length() - 1);
  if (u.length() && u != _sbUrl)             { _sbUrl = u;       _nvs.putString("sburl", u);     change = true; }
  if (cle.length() && cle != _sbCle)         { _sbCle = cle;     _nvs.putString("sbcle", cle);   change = true; }
  if (table.length() && table != _sbTable)   { _sbTable = table; _nvs.putString("sbtab", table); change = true; }
  return change;
}

bool Reglages::definirBroche(const char* nom, int v) {
  if (v < -1 || v > 48) return false;
  int8_t* cible = nullptr;
  const char* cleNvs = nullptr;
  if (!strcmp(nom, "sda"))          { cible = &_sda;     cleNvs = "bsda"; }
  else if (!strcmp(nom, "scl"))     { cible = &_scl;     cleNvs = "bscl"; }
  else if (!strcmp(nom, "secteur")) { cible = &_secteur; cleNvs = "bsect"; }
  if (!cible || *cible == v) return false;
  *cible = (int8_t)v;
  _nvs.putChar(cleNvs, *cible);
  return true;
}

bool Reglages::definirJournal(bool actif) {
  if (actif == _journal) return false;
  _journal = actif;
  _nvs.putBool("journal", actif);
  return true;
}
