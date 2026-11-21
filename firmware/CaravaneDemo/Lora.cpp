// ==== Caravane =======================================================================
// Lora.cpp — firmware CaravaneDemo, version 0.10.0 — dernière modification : 2026-11-21
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
#include "Lora.h"
#include "Journal.h"
#include "Releves.h"

// ---------- Trame binaire d'un relevé (port 1) ----------
// Petit-boutiste. Voir docs/donnees.md et ttn/decodeur.js.
//   0      version du format (1)
//   1..4   n° de relevé (uint32)
//   5      drapeaux : bit 0 = secteur
//   6      masque des grandeurs présentes (bit 7 réservé : 2e octet de masque)
//   ...    valeurs présentes, dans l'ordre des bits :
//          bit 0 temp_c    int16  ×10
//          bit 1 hum_pct   uint8  ×2
//          bit 2 press_hpa uint16 (hPa − 500) ×10
//          bit 3 eco2_ppm  uint16
//          bit 4 tvoc_ppb  uint16
//          bit 5 vbat_mv   uint8  (mV − 2500) / 10
size_t trameReleve(const Releve& r, uint8_t* t) {
  size_t n = 0;
  auto u8  = [&](uint8_t v)  { t[n++] = v; };
  auto u16 = [&](uint16_t v) { t[n++] = v & 0xFF; t[n++] = v >> 8; };
  u8(1);
  u16(r.numero & 0xFFFF); u16(r.numero >> 16);
  u8(r.secteur() ? 0x01 : 0x00);
  size_t posMasque = n;
  u8(0);
  uint8_t masque = 0;
  if (r.temp_c10 != INT16_MIN) { masque |= 0x01; u16((uint16_t)r.temp_c10); }
  if (r.hum_c10 != 0xFFFF)     { masque |= 0x02; u8((uint8_t)constrain(lroundf(r.hum_c10 / 5.0f), 0L, 200L)); }
  if (r.press_hpa10 != 0xFFFF) { masque |= 0x04; u16((uint16_t)constrain((long)r.press_hpa10 - 5000L, 0L, 65535L)); }
  if (r.eco2_ppm)              { masque |= 0x08; u16(r.eco2_ppm); masque |= 0x10; u16(r.tvoc_ppb); }
  if (r.vbat_mv)               { masque |= 0x20; u8((uint8_t)constrain(((long)r.vbat_mv - 2500L) / 10L, 0L, 255L)); }
  t[posMasque] = masque;
  return n;
}

#if !CONFIG_IDF_TARGET_ESP32S3

// ---------- V2 : pas de radio LoRa ----------
bool Lora::demarrer(const String&, const String&, const String&) { return false; }
bool Lora::envoyer(const uint8_t*, size_t, uint8_t) { return false; }
void Lora::sauverSession() {}
void Lora::oublier(bool) {}
bool Lora::connecter() { return false; }
String Lora::texteEtat() const { return "absent (carte sans LoRa)"; }

#else

#include <RadioLib.h>
#include <SPI.h>
#include <Preferences.h>

// Heltec WiFi LoRa 32 V3 : SX1262 sur SPI (SCK 9, MISO 11, MOSI 10), NSS 8, DIO1 14, RESET 12, BUSY 13
static SX1262 radio = new Module(8, 14, 12, 13);
static LoRaWANNode noeud(&radio, &EU868);

// Session LoRaWAN conservée pendant la veille profonde (la mémoire RTC n'est perdue qu'à la
// coupure d'alimentation ou au téléversement)
RTC_DATA_ATTR static uint8_t sessionRtc[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
RTC_DATA_ATTR static bool sessionRtcValide = false;
// Échecs de connexion successifs : on espace les tentatives pour économiser la batterie
RTC_DATA_ATTR static uint8_t echecsConnexion = 0;
RTC_DATA_ATTR static uint16_t tentativesSautees = 0;

static uint64_t hexVersU64(const String& h) {
  uint64_t v = 0;
  for (size_t i = 0; i < h.length(); i++) {
    char c = h[i];
    uint8_t d = isdigit(c) ? c - '0' : (toupper(c) - 'A' + 10);
    v = (v << 4) | (d & 0x0F);
  }
  return v;
}

static bool hexVersOctets(const String& h, uint8_t* sortie, size_t n) {
  if (h.length() != n * 2) return false;
  for (size_t i = 0; i < n; i++) {
    char tmp[3] = { h[2 * i], h[2 * i + 1], 0 };
    if (!isxdigit(tmp[0]) || !isxdigit(tmp[1])) return false;
    sortie[i] = (uint8_t)strtoul(tmp, nullptr, 16);
  }
  return true;
}

static uint8_t appKey[16];

bool Lora::demarrer(const String& devEuiHex, const String& joinEuiHex, const String& appKeyHex) {
  _pret = false;
  if (!hexVersOctets(appKeyHex, appKey, 16)) {
    journalln("LoRa : AppKey absente ou invalide (à saisir depuis le téléphone)");
    return false;
  }
  SPI.begin(9, 11, 10, 8);
  int16_t e = radio.begin(868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8, 1.8, false);
  if (e != RADIOLIB_ERR_NONE) {
    journalf("LoRa : radio introuvable (code %d)\n", e);
    _etat.code = e;
    return false;
  }
  // LoRaWAN 1.0.x : une seule clé (AppKey), nwkKey = NULL
  e = noeud.beginOTAA(hexVersU64(joinEuiHex), hexVersU64(devEuiHex), nullptr, appKey);
  if (e != RADIOLIB_ERR_NONE) {
    journalf("LoRa : identifiants refusés (code %d)\n", e);
    _etat.code = e;
    return false;
  }
  // Nonces de connexion gardés en flash
  Preferences nvs;
  nvs.begin("lorawan", true);
  if (nvs.getBytesLength("nonces") == RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    uint8_t tampon[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    nvs.getBytes("nonces", tampon, sizeof(tampon));
    e = noeud.setBufferNonces(tampon);
    if (e != RADIOLIB_ERR_NONE) journalf("LoRa : nonces ignorés (code %d, clé changée ?)\n", e);
  }
  nvs.end();
  // Session gardée pendant la veille
  if (sessionRtcValide) {
    e = noeud.setBufferSession(sessionRtc);
    if (e != RADIOLIB_ERR_NONE) { sessionRtcValide = false; journalf("LoRa : session ignorée (code %d)\n", e); }
  }
  _pret = true;
  return true;
}

bool Lora::connecter() {
  if (noeud.isActivated()) return true;
  // Après 3 échecs, une tentative seulement un réveil sur 4 (puis sur 8, 16 au plus)
  if (echecsConnexion >= 3) {
    uint16_t pas = 1U << min<uint8_t>(echecsConnexion - 1, 4);
    if (++tentativesSautees < pas) {
      journalf("LoRa : connexion reportée (%u échecs, prochain essai dans %u réveil(s))\n",
               echecsConnexion, pas - tentativesSautees);
      return false;
    }
    tentativesSautees = 0;
  }
  if (!sessionRtcValide) journalln("LoRa : connexion au réseau (join)…");
  uint32_t t0 = millis();
  int16_t e = noeud.activateOTAA();
  // Nonces à garder après chaque vraie tentative de connexion, réussie ou non (inutile quand la
  // session est seulement restaurée depuis la mémoire RTC)
  if (e != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    Preferences nvs;
    nvs.begin("lorawan", false);
    nvs.putBytes("nonces", noeud.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
    nvs.end();
  }
  _etat.code = e;
  if (e == RADIOLIB_LORAWAN_NEW_SESSION || e == RADIOLIB_LORAWAN_SESSION_RESTORED) {
    journalf("LoRa : %s en %lu ms, adresse %08lX\n",
             e == RADIOLIB_LORAWAN_NEW_SESSION ? "connecté" : "session restaurée",
             (unsigned long)(millis() - t0), (unsigned long)noeud.getDevAddr());
    echecsConnexion = 0;
    tentativesSautees = 0;
    noeud.setADR(true);
    noeud.setDutyCycle(true, 1250);                  // politique d'usage équitable TTN (30 s/jour)
    sauverSession();
    return true;
  }
  if (echecsConnexion < 255) echecsConnexion++;
  journalf("LoRa : échec de connexion (code %d%s) en %lu ms\n", e,
           e == RADIOLIB_ERR_NO_JOIN_ACCEPT ? ", pas de réponse : passerelle hors de portée, ou clés différentes dans TTN" : "",
           (unsigned long)(millis() - t0));
  return false;
}

bool Lora::envoyer(const uint8_t* donnees, size_t taille, uint8_t port) {
  if (!_pret) return false;
  _etat.quandMs = millis();
  if (!connecter()) { _etat.connecte = false; return false; }
  _etat.connecte = true;
  uint32_t attente = noeud.timeUntilUplink();
  if (attente > 0) {
    journalf("LoRa : envoi reporté (temps d'antenne : encore %lu s à attendre)\n", (unsigned long)(attente / 1000));
    _etat.code = RADIOLIB_ERR_UPLINK_UNAVAILABLE;
    return false;
  }
  LoRaWANEvent_t montant, descendant;
  uint8_t recu[32];
  size_t tailleRecue = 0;
  int16_t e = noeud.sendReceive(donnees, taille, port, recu, &tailleRecue, false, &montant, &descendant);
  _etat.code = e < 0 ? e : 0;
  _etat.toaMs = noeud.getLastToA();
  _etat.trames = noeud.getFCntUp();
  if (e > 0) { _etat.rssi = (int16_t)radio.getRSSI(); _etat.snr = radio.getSNR(); }
  sauverSession();
  if (e < RADIOLIB_ERR_NONE) {
    journalf("LoRa : échec d'envoi (code %d)\n", e);
    if (e == RADIOLIB_ERR_NETWORK_NOT_JOINED || e == RADIOLIB_ERR_SESSION_DISCARDED) oublier(false);
    return false;
  }
  journalf("LoRa : trame n°%lu envoyée (%u octets, port %u, %lu ms d'antenne, DR%u)%s\n",
           (unsigned long)montant.fCnt, (unsigned)taille, port, (unsigned long)_etat.toaMs, montant.datarate,
           e > 0 ? ", réponse du réseau reçue" : "");
  return true;
}

void Lora::sauverSession() {
  if (!noeud.isActivated()) return;
  memcpy(sessionRtc, noeud.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  sessionRtcValide = true;
}

void Lora::oublier(bool aussiNonces) {
  noeud.clearSession();
  sessionRtcValide = false;
  echecsConnexion = 0;
  tentativesSautees = 0;
  _etat.connecte = false;
  if (!aussiNonces) return;
  Preferences nvs;
  nvs.begin("lorawan", false);
  nvs.clear();
  nvs.end();
}

String Lora::texteEtat() const {
  if (!_pret) return _etat.code ? "erreur radio (" + String(_etat.code) + ")" : "clé manquante";
  if (noeud.isActivated()) return "connecté";
  if (_etat.quandMs == 0) return "pas encore connecté";
  return "non connecté (" + String(_etat.code) + ")";
}

#endif
