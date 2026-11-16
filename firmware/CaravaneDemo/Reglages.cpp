#include "Reglages.h"
#include "Config.h"

static const char* ESPACE_NVS = "caravane";

void Reglages::charger() {
  _nvs.begin(ESPACE_NVS, false);
  _message     = _nvs.getString("msg", MESSAGE_DEFAUT);
  _inactiviteS = _nvs.getULong("inact", INACTIVITE_DEFAUT_S);
  _veilleS     = _nvs.getULong("veille", VEILLE_DEFAUT_S);
  _fuseauMin   = _nvs.getShort("tz", FUSEAU_DEFAUT_MIN);
}

void Reglages::reinitialiser() {
  _nvs.clear();
  _message     = MESSAGE_DEFAUT;
  _inactiviteS = INACTIVITE_DEFAUT_S;
  _veilleS     = VEILLE_DEFAUT_S;
  _fuseauMin   = FUSEAU_DEFAUT_MIN;
}

bool Reglages::definirMessage(const String& texte) {
  String t = texte;
  t.trim();
  if (t.length() == 0) return false;                       // champ vide : on ne touche à rien
  if (t.length() > MESSAGE_LONGUEUR_MAX) t = t.substring(0, MESSAGE_LONGUEUR_MAX);
  if (t == _message) return false;
  _message = t;
  _nvs.putString("msg", _message);
  return true;
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
