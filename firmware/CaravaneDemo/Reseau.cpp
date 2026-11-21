// ==== Caravane =======================================================================
// Reseau.cpp — firmware CaravaneDemo, version 0.9.3 — dernière modification : 2026-11-19
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
#include "Reseau.h"
#include "Journal.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

void Reseau::demarrer(const String& ssid, const String& mdp) {
  if (ssid.length() == 0) { arreter(); return; }
  if (_actif && ssid == _ssid && mdp == _mdp) return;
  _ssid = ssid;
  _mdp = mdp;
  WiFi.persistent(false);              // les identifiants sont gardés par Reglages, pas par le Wi-Fi
  WiFi.mode(WIFI_STA);
  // Pas d'économie d'énergie radio en mode Wi-Fi (la carte est alors sur secteur ou reste
  // éveillée de toute façon) : en veille modem, la poignée de main TLS, longue sur la V2,
  // perd des paquets et le serveur coupe (erreurs -1 / -5 « EOF » après 15 s).
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect();
  WiFi.begin(_ssid.c_str(), _mdp.c_str());
  _actif = true;
  _ntpLance = false;
  _etaitConnecte = false;
  _depuisMs = millis();
  journalf("Wi-Fi : connexion à « %s »\n", _ssid.c_str());
}

void Reseau::arreter() {
  if (!_actif) return;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  _actif = false;
  _ntpLance = false;
  journalln("Wi-Fi : arrêté");
}

const char* Reseau::etat() const {
  if (!_actif) return "inactif";
  return WiFi.status() == WL_CONNECTED ? "connecte" : "connexion";
}

void Reseau::entretenir() {
  if (!_actif) return;
  bool c = WiFi.status() == WL_CONNECTED;
  if (!c) {
    if (_etaitConnecte) {               // connexion perdue
      journalln("Wi-Fi : connexion perdue");
      _etaitConnecte = false;
      _depuisMs = millis();
    }
    // Pas connecté au bout de 30 s : on relance la connexion (sans cela, elle peut rester bloquée)
    if (millis() - _depuisMs > 30000) {
      journalf("Wi-Fi : pas de connexion après 30 s (statut %d), nouvel essai\n", (int)WiFi.status());
      WiFi.disconnect();
      delay(100);
      WiFi.begin(_ssid.c_str(), _mdp.c_str());
      _depuisMs = millis();
    }
    return;
  }
  _etaitConnecte = true;
  if (_ntpLance) return;
  // Heure système en UTC ; l'affichage applique le fuseau des réglages.
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  _ntpLance = true;
  journalf("Wi-Fi : connecté, IP %s, %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

String Reseau::scanner() {
  bool etaitActif = _actif;
  if (!etaitActif) WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks(false, false);
  JsonDocument doc;
  JsonArray liste = doc["wifi_reseaux"].to<JsonArray>();
  // Tri par puissance décroissante, sans doublons, 15 réseaux au plus
  for (int k = 0; k < 15; k++) {
    int meilleur = -1;
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i).length() == 0) continue;
      bool dejaVu = false;
      for (JsonObject r : liste) if (WiFi.SSID(i) == r["ssid"].as<const char*>()) { dejaVu = true; break; }
      if (dejaVu) continue;
      if (meilleur < 0 || WiFi.RSSI(i) > WiFi.RSSI(meilleur)) meilleur = i;
    }
    if (meilleur < 0) break;
    JsonObject r = liste.add<JsonObject>();
    r["ssid"] = WiFi.SSID(meilleur);
    r["rssi"] = WiFi.RSSI(meilleur);
    r["ouvert"] = WiFi.encryptionType(meilleur) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  if (!etaitActif) WiFi.mode(WIFI_OFF);
  String sortie;
  serializeJson(doc, sortie);
  return sortie;
}

int Reseau::envoyerSupabase(const String& url, const String& cle, const String& table, const String& lignesJson,
                            uint16_t nombre) {
  _dernier.tente = true;
  _dernier.quandMs = millis();
  if (!connecte()) { _dernier.code = -100; return _dernier.code; }   // -100 : Wi-Fi non connecté

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(30);     // s : la poignée de main TLS est lente sur l'ESP32 (V2)
  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(20000);
  if (!http.begin(client, url + "/rest/v1/" + table)) {
    _dernier.code = -101;
    return _dernier.code;
  }
  http.addHeader("apikey", cle);
  http.addHeader("Authorization", "Bearer " + cle);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  uint32_t debut = millis();
  int code = http.POST(lignesJson);
  journalf("Supabase : réponse %d en %lu ms (%u octets envoyés)\n", code, (unsigned long)(millis() - debut),
                (unsigned)lignesJson.length());
  if (code < 0) {
    // Diagnostic : adresse résolue ? mémoire disponible pour le chiffrement ? erreur TLS ?
    char erreur[100] = "";
    client.lastError(erreur, sizeof(erreur));
    IPAddress ip;
    String hote = url;
    hote.replace("https://", "");
    bool dns = WiFi.hostByName(hote.c_str(), ip);
    // Test réseau brut : une simple connexion TCP (sans chiffrement) au port 443 de Supabase
    // et au port 80 d'un site public dit si la carte sort vraiment sur Internet.
    WiFiClient brut;
    bool tcp443 = dns && brut.connect(ip, 443, 5000);
    brut.stop();
    bool tcp80 = brut.connect("example.com", 80, 5000);
    brut.stop();
    journalf("Supabase : échec de connexion (%d) vers %s, DNS %s, TLS « %s », mémoire libre %u o (bloc max %u o)\n",
                  code, hote.c_str(), dns ? ip.toString().c_str() : "ÉCHEC", erreur,
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    // Poignée de main TLS seule, chronométrée
    WiFiClientSecure tls;
    tls.setInsecure();
    tls.setHandshakeTimeout(30);
    uint32_t t0 = millis();
    bool tlsOk = tls.connect(hote.c_str(), 443);
    uint32_t dureeTls = millis() - t0;
    char errTls[100] = "";
    if (!tlsOk) tls.lastError(errTls, sizeof(errTls));
    tls.stop();
    journalf("Réseau : TLS seul %s en %lu ms %s, Wi-Fi %d dBm\n", tlsOk ? "OK" : "ÉCHEC",
                  (unsigned long)dureeTls, errTls, WiFi.RSSI());
    journalf("Réseau : TCP %s:443 %s, TCP example.com:80 %s, heure Internet %s, passerelle %s, DNS %s\n",
                  ip.toString().c_str(), tcp443 ? "OK" : "ÉCHEC", tcp80 ? "OK" : "ÉCHEC",
                  time(nullptr) > 1700000000 ? "réglée" : "NON réglée",
                  WiFi.gatewayIP().toString().c_str(), WiFi.dnsIP().toString().c_str());
  }
  if (code != 201 && code != 409) journalf("Supabase : HTTP %d %s\n", code, http.getString().c_str());
  else journalf("Supabase : %u relevé(s) publié(s)\n", nombre);
  http.end();
  _dernier.code = code;
  if (_dernier.ok()) _dernier.nombre = nombre;
  return code;
}
