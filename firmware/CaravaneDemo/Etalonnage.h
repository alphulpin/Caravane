// ==== Caravane =======================================================================
// Etalonnage.h — firmware CaravaneDemo, version 0.10.2 — dernière modification : 2026-11-17
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Etalonnage — correction linéaire de chaque grandeur mesurée : valeur = pente × brute + décalage.
// Défaut : pente 1, décalage 0 (aucune correction). Réglé depuis le téléphone, gardé en flash.
// Les relevés stockés et publiés sont les valeurs corrigées ; l'étalonnage en vigueur est publié
// à part (table caravane_config), ce qui permet de retrouver la valeur brute si besoin.
#pragma once
#include <Arduino.h>

enum Grandeur : uint8_t { G_TEMP, G_HUM, G_PRESS, G_ECO2, G_TVOC, G_VBAT, G_NOMBRE };

struct Etalon {
  float pente = 1.0f;
  float decalage = 0.0f;
  bool parDefaut() const { return pente == 1.0f && decalage == 0.0f; }
};

// Codes = clés JSON des mesures (identiques dans Supabase et sur le téléphone)
inline const char* codeGrandeur(uint8_t g) {
  static const char* codes[G_NOMBRE] = { "temp_c", "hum_pct", "press_hpa", "eco2_ppm", "tvoc_ppb", "vbat_mv" };
  return g < G_NOMBRE ? codes[g] : "";
}
inline int grandeurDepuisCode(const char* code) {
  for (uint8_t g = 0; g < G_NOMBRE; g++) if (strcmp(code, codeGrandeur(g)) == 0) return g;
  return -1;
}

// Bornes acceptées (protection contre une faute de frappe)
constexpr float PENTE_MIN = 0.5f, PENTE_MAX = 2.0f;
constexpr float DECALAGE_MAX[G_NOMBRE] = { 10.0f, 20.0f, 50.0f, 500.0f, 500.0f, 500.0f };

class Etalonnage {
public:
  const Etalon& operator[](uint8_t g) const { return _e[g < G_NOMBRE ? g : 0]; }
  float appliquer(uint8_t g, float brute) const {
    return isnan(brute) ? brute : _e[g].pente * brute + _e[g].decalage;
  }
  // Renvoie true si la valeur a changé
  bool definir(uint8_t g, float pente, float decalage) {
    if (g >= G_NOMBRE || isnan(pente) || isnan(decalage)) return false;
    pente = constrain(pente, PENTE_MIN, PENTE_MAX);
    decalage = constrain(decalage, -DECALAGE_MAX[g], DECALAGE_MAX[g]);
    pente = roundf(pente * 10000.0f) / 10000.0f;       // 4 décimales
    decalage = roundf(decalage * 100.0f) / 100.0f;     // 2 décimales
    if (pente == _e[g].pente && decalage == _e[g].decalage) return false;
    _e[g].pente = pente;
    _e[g].decalage = decalage;
    return true;
  }
  Etalon* donnees() { return _e; }
  static constexpr size_t TAILLE = sizeof(Etalon) * G_NOMBRE;

private:
  Etalon _e[G_NOMBRE];
};
