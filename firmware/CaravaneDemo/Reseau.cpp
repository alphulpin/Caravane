#include "Reseau.h"
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
  WiFi.setAutoReconnect(true);
  WiFi.disconnect();
  WiFi.begin(_ssid.c_str(), _mdp.c_str());
  _actif = true;
  _ntpLance = false;
  Serial.printf("Wi-Fi : connexion à « %s »\n", _ssid.c_str());
}

void Reseau::arreter() {
  if (!_actif) return;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  _actif = false;
  _ntpLance = false;
  Serial.println("Wi-Fi : arrêté");
}

const char* Reseau::etat() const {
  if (!_actif) return "inactif";
  return WiFi.status() == WL_CONNECTED ? "connecte" : "connexion";
}

void Reseau::entretenir() {
  if (!connecte() || _ntpLance) return;
  // Heure système en UTC ; l'affichage applique le fuseau des réglages.
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  _ntpLance = true;
  Serial.printf("Wi-Fi : connecté, IP %s, %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
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
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url + "/rest/v1/" + table)) {
    _dernier.code = -101;
    return _dernier.code;
  }
  http.addHeader("apikey", cle);
  http.addHeader("Authorization", "Bearer " + cle);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  int code = http.POST(lignesJson);
  if (code < 0) {
    // Diagnostic : adresse résolue ? mémoire disponible pour le chiffrement ? erreur TLS ?
    char erreur[100] = "";
    client.lastError(erreur, sizeof(erreur));
    IPAddress ip;
    String hote = url;
    hote.replace("https://", "");
    bool dns = WiFi.hostByName(hote.c_str(), ip);
    Serial.printf("Supabase : échec de connexion (%d) vers %s, DNS %s, TLS « %s », mémoire libre %u o (bloc max %u o)\n",
                  code, hote.c_str(), dns ? ip.toString().c_str() : "ÉCHEC", erreur,
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  }
  if (code != 201 && code != 409) Serial.printf("Supabase : HTTP %d %s\n", code, http.getString().c_str());
  else Serial.printf("Supabase : %u relevé(s) publié(s)\n", nombre);
  http.end();
  _dernier.code = code;
  if (_dernier.ok()) _dernier.nombre = nombre;
  return code;
}
