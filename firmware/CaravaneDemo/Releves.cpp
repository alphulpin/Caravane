// ==== Caravane =======================================================================
// Releves.cpp — firmware CaravaneDemo, version 0.10.1 — dernière modification : 2026-11-19
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
#include "Releves.h"
#include "Journal.h"
#include <LittleFS.h>

static const char*    CHEMIN = "/releves.bin";
static const uint32_t MAGIQUE = 0x31565243;   // "CRV1"

struct __attribute__((packed)) Entete {
  uint32_t magique;
  uint16_t tailleReleve;
  uint16_t capacite;
  uint16_t debut;        // emplacement du plus ancien relevé
  uint16_t nombre;       // relevés présents
  uint32_t reserve;
};
static_assert(sizeof(Entete) == 16, "taille de l'en-tête");

size_t Releves::position(uint16_t index) const {
  return sizeof(Entete) + (size_t)index * sizeof(Releve);
}

bool Releves::ecrireEntete(File& f) {
  Entete e = { MAGIQUE, sizeof(Releve), RELEVES_CAPACITE, _debut, _nombre, 0 };
  return f.seek(0) && f.write((const uint8_t*)&e, sizeof(e)) == sizeof(e);
}

bool Releves::creer() {
  File f = LittleFS.open(CHEMIN, "w");
  if (!f) return false;
  _debut = _nombre = _aPublier = 0;
  ecrireEntete(f);
  // Réserve toute la place d'un coup : le fichier ne grossira plus ensuite.
  uint8_t zeros[sizeof(Releve) * 10] = {};
  for (uint16_t i = 0; i < RELEVES_CAPACITE; i += 10) f.write(zeros, sizeof(zeros));
  f.close();
  journalf("Relevés : journal créé (%u emplacements)\n", RELEVES_CAPACITE);
  return true;
}

bool Releves::demarrer() {
  // Partition « spiffs » des deux cartes ; formatée au premier démarrage si besoin.
  if (!LittleFS.begin(true)) { journalln("Relevés : LittleFS indisponible"); return _pret = false; }
  File f = LittleFS.open(CHEMIN, "r");
  Entete e = {};
  bool valide = f && f.read((uint8_t*)&e, sizeof(e)) == sizeof(e) && e.magique == MAGIQUE &&
                e.tailleReleve == sizeof(Releve) && e.capacite == RELEVES_CAPACITE &&
                e.debut < RELEVES_CAPACITE && e.nombre <= RELEVES_CAPACITE &&
                f.size() >= position(RELEVES_CAPACITE);
  if (!valide) {
    if (f) f.close();
    return _pret = creer();
  }
  _debut = e.debut;
  _nombre = e.nombre;
  _aPublier = 0;
  Releve r;
  for (uint16_t i = 0; i < _nombre; i++) {
    f.seek(position((_debut + i) % RELEVES_CAPACITE));
    if (f.read((uint8_t*)&r, sizeof(r)) == sizeof(r) && !r.publie()) _aPublier++;
  }
  f.close();
  journalf("Relevés : %u en mémoire, %u à publier\n", _nombre, _aPublier);
  return _pret = true;
}

bool Releves::ajouter(const Releve& r, uint16_t* indexAjoute) {
  if (!_pret) return false;
  File f = LittleFS.open(CHEMIN, "r+");
  if (!f) return false;
  uint16_t index;
  if (_nombre < RELEVES_CAPACITE) {
    index = (_debut + _nombre) % RELEVES_CAPACITE;
    _nombre++;
  } else {                                     // plein : on remplace le plus ancien
    index = _debut;
    Releve ancien;
    f.seek(position(index));
    if (f.read((uint8_t*)&ancien, sizeof(ancien)) == sizeof(ancien) && !ancien.publie() && _aPublier) _aPublier--;
    _debut = (_debut + 1) % RELEVES_CAPACITE;
  }
  f.seek(position(index));
  bool ok = f.write((const uint8_t*)&r, sizeof(r)) == sizeof(r);
  ok = ecrireEntete(f) && ok;
  f.close();
  if (ok && !r.publie()) _aPublier++;
  if (ok && indexAjoute) *indexAjoute = index;
  return ok;
}

bool Releves::dater(uint16_t index, uint32_t horodatage) {
  if (!_pret || index >= RELEVES_CAPACITE) return false;
  File f = LittleFS.open(CHEMIN, "r+");
  if (!f) return false;
  const size_t decalage = offsetof(Releve, horodatage);
  uint32_t actuel = 1;
  bool ok = f.seek(position(index) + decalage) && f.read((uint8_t*)&actuel, 4) == 4 && actuel == 0 &&
            f.seek(position(index) + decalage) && f.write((const uint8_t*)&horodatage, 4) == 4;
  f.close();
  return ok;
}

size_t Releves::nonPublies(Releve* sortie, uint16_t* index, size_t max) {
  if (!_pret || _aPublier == 0 || max == 0) return 0;
  File f = LittleFS.open(CHEMIN, "r");
  if (!f) return 0;
  size_t n = 0;
  Releve r;
  for (uint16_t i = 0; i < _nombre && n < max; i++) {
    uint16_t idx = (_debut + i) % RELEVES_CAPACITE;
    f.seek(position(idx));
    if (f.read((uint8_t*)&r, sizeof(r)) != sizeof(r)) break;
    if (r.publie()) continue;
    sortie[n] = r;
    index[n] = idx;
    n++;
  }
  f.close();
  return n;
}

bool Releves::dernier(Releve& r, uint16_t& index) {
  if (!_pret || _nombre == 0) return false;
  File f = LittleFS.open(CHEMIN, "r");
  if (!f) return false;
  index = (_debut + _nombre - 1) % RELEVES_CAPACITE;
  f.seek(position(index));
  bool ok = f.read((uint8_t*)&r, sizeof(r)) == sizeof(r);
  f.close();
  return ok;
}

void Releves::marquerPublies(const uint16_t* index, size_t n) {
  if (!_pret || n == 0) return;
  File f = LittleFS.open(CHEMIN, "r+");
  if (!f) return;
  const size_t decalage = offsetof(Releve, drapeaux);
  for (size_t i = 0; i < n; i++) {
    uint8_t d;
    f.seek(position(index[i]) + decalage);
    if (f.read(&d, 1) != 1 || (d & Releve::PUBLIE)) continue;
    d |= Releve::PUBLIE;
    f.seek(position(index[i]) + decalage);
    if (f.write(&d, 1) == 1 && _aPublier) _aPublier--;
  }
  f.close();                                   // une seule validation pour tout le lot
}

void Releves::effacer() {
  if (!_pret) return;
  File f = LittleFS.open(CHEMIN, "r+");
  if (!f) return;
  _debut = _nombre = _aPublier = 0;
  ecrireEntete(f);
  f.close();
  journalln("Relevés : journal vidé");
}
