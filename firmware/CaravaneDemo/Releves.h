// ==== Caravane =======================================================================
// Releves.h — firmware CaravaneDemo, version 0.9.2 — dernière modification : 2026-11-17
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Releves — journal des relevés en mémoire flash (LittleFS), avec une marque « publié ».
//
// Fichier /releves.bin : un en-tête puis RELEVES_CAPACITE emplacements de 24 octets,
// utilisés en anneau (quand c'est plein, le plus ancien est remplacé, publié ou non).
// 1000 relevés = 24 Ko : rien à craindre pour la mémoire (partition de 1 Mo au moins).
// Usure de la flash : une écriture par relevé et par publication, négligeable à 4 relevés par heure.
#pragma once
#include <Arduino.h>
#include <FS.h>
#include "Config.h"

struct __attribute__((packed)) Releve {
  uint32_t numero;        // numéro croissant propre à la carte (clé d'unicité dans Supabase)
  uint32_t horodatage;    // epoch UTC ; 0 = heure inconnue
  int16_t  temp_c10;      // °C x 10 ; INT16_MIN = absent
  uint16_t hum_c10;       // % x 10 ; 0xFFFF = absent
  uint16_t press_hpa10;   // hPa x 10 ; 0xFFFF = absent
  uint16_t eco2_ppm;      // 0 = absent
  uint16_t tvoc_ppb;      // valable si eco2_ppm != 0
  uint16_t vbat_mv;       // 0 = absent
  uint8_t  drapeaux;      // bit 0 : secteur ; bit 1 : publié
  uint8_t  reserve[3];

  static constexpr uint8_t SECTEUR = 0x01;
  static constexpr uint8_t PUBLIE  = 0x02;
  bool publie() const  { return drapeaux & PUBLIE; }
  bool secteur() const { return drapeaux & SECTEUR; }
};
static_assert(sizeof(Releve) == 24, "taille de Releve");

class Releves {
public:
  bool demarrer();                        // monte LittleFS, ouvre ou crée le journal
  bool pret() const { return _pret; }

  bool ajouter(const Releve& r, uint16_t* indexAjoute = nullptr);
  // Donne une date à un relevé pris avant que l'horloge soit réglée (sans effet s'il en a déjà une).
  bool dater(uint16_t index, uint32_t horodatage);
  // Les plus anciens relevés non publiés (max au plus) ; index[] sert à les marquer ensuite.
  size_t nonPublies(Releve* sortie, uint16_t* index, size_t max);
  bool dernier(Releve& r, uint16_t& index);
  void marquerPublies(const uint16_t* index, size_t n);
  void effacer();

  uint16_t nombre() const   { return _nombre; }
  uint16_t aPublier() const { return _aPublier; }
  uint16_t capacite() const { return RELEVES_CAPACITE; }

private:
  bool creer();
  bool ecrireEntete(File& f);
  size_t position(uint16_t index) const;

  bool _pret = false;
  uint16_t _debut = 0, _nombre = 0, _aPublier = 0;
};
