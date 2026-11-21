// ==== Caravane =======================================================================
// Lora.h — firmware CaravaneDemo, version 0.10.1 — dernière modification : 2026-11-21
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Lora — liaison LoRaWAN vers The Things Network (Heltec V3, puce SX1262), avec RadioLib 7.8.
//
// Activation OTAA (LoRaWAN 1.0.3) : DevEUI tiré du n° de série, JoinEUI et AppKey saisis depuis le
// téléphone. Pour ne pas refaire la connexion (« join ») à chaque réveil :
//   - la session (clés, compteurs) est gardée en mémoire RTC, qui survit à la veille profonde ;
//   - les « nonces » de connexion sont gardés en flash (NVS), comme l'exige LoRaWAN : un même
//     DevNonce ne peut pas resservir, même après une coupure d'alimentation.
// Sur la V2 (pas de radio LoRa), la classe existe mais ne fait rien.
#pragma once
#include <Arduino.h>
#include "Config.h"

// Résultat du dernier échange, pour le téléphone et le journal
struct EtatLora {
  bool connecte = false;        // session LoRaWAN active
  int16_t code = 0;             // dernier code RadioLib (0 = OK)
  uint32_t quandMs = 0;         // millis() du dernier envoi tenté (0 = jamais)
  uint32_t toaMs = 0;           // temps d'antenne du dernier envoi
  uint32_t trames = 0;          // compteur de trames montantes (FCntUp)
  int16_t rssi = 0;             // signal reçu de la passerelle (s'il y a eu une réponse)
  float snr = 0;
};

class Lora {
public:
  // Prépare la radio et les identifiants. appKeyHex : 32 caractères hexadécimaux.
  bool demarrer(const String& devEuiHex, const String& joinEuiHex, const String& appKeyHex);
  bool pret() const { return _pret; }

  // Envoie une trame (port 1 = relevé). Restaure la session ou se connecte si besoin.
  // Renvoie true si la trame est partie.
  bool envoyer(const uint8_t* donnees, size_t taille, uint8_t port);

  // À appeler juste avant la veille profonde (copie la session en mémoire RTC)
  void sauverSession();
  // Oublie la session ; avec aussiNonces, oublie aussi les nonces (nouvelle clé : la carte repart
  // de zéro, il faut alors aussi réinitialiser les nonces de l'appareil dans TTN)
  void oublier(bool aussiNonces);

  const EtatLora& etat() const { return _etat; }
  String texteEtat() const;     // « connecté », « non connecté (code) », « clé manquante »…

private:
  bool connecter();
  bool _pret = false;
  EtatLora _etat;
};

// Trame binaire d'un relevé (port 1). Format décrit dans docs/donnees.md, décodée par
// ttn/decodeur.js. Renvoie la taille écrite dans « sortie » (24 octets suffisent).
struct Releve;
size_t trameReleve(const Releve& r, uint8_t* sortie);
