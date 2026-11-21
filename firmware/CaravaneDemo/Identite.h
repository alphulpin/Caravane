// ==== Caravane =======================================================================
// Identite.h — firmware CaravaneDemo, version 0.10.1 — dernière modification : 2026-11-17
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Identite — numéros qui identifient la carte, sans rien avoir à régler.
//
// - N° de série : l'adresse MAC gravée en usine dans la puce (48 bits), unique au monde.
//   Écrit en 12 chiffres hexadécimaux, ex. "34B7DA5C1E20". C'est la clé "appareil" dans Supabase.
// - DevEUI (LoRaWAN, V3) : identifiant 64 bits de l'objet sur le réseau LoRa, à déclarer dans TTN.
//   On le tire de l'adresse MAC en insérant FFFE au milieu (règle EUI-48 -> EUI-64) :
//   34B7DA5C1E20 -> 34B7DAFFFE5C1E20. Le n° de série reste donc lisible dans le DevEUI.
//   (Le DevAddr que TTN attribue à chaque connexion change : ce n'est pas un identifiant.)
#pragma once
#include <Arduino.h>

class Identite {
public:
  static String numeroSerie() {
    uint8_t m[6];
    octetsMac(m);
    char t[13];
    snprintf(t, sizeof(t), "%02X%02X%02X%02X%02X%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return t;
  }

  static String devEui() {
    uint8_t m[6];
    octetsMac(m);
    char t[17];
    snprintf(t, sizeof(t), "%02X%02X%02XFFFE%02X%02X%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return t;
  }

private:
  // ESP.getEfuseMac() range le premier octet de l'adresse dans les bits de poids faible.
  static void octetsMac(uint8_t m[6]) {
    uint64_t mac = ESP.getEfuseMac();
    for (uint8_t i = 0; i < 6; i++) m[i] = (mac >> (8 * i)) & 0xFF;
  }
};
