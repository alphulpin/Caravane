// ==== Caravane =======================================================================
// Reseau.h — firmware CaravaneDemo, version 0.10.1 — dernière modification : 2026-11-17
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Reseau — Wi-Fi (connexion, recherche des réseaux, mise à l'heure NTP) et écriture
// directe dans Supabase par l'API REST (PostgREST).
//
// Supabase : POST https://<projet>.supabase.co/rest/v1/<table>, en-têtes apikey + Authorization.
// La clé utilisée ne doit avoir que le droit d'insérer (voir supabase/caravane_releves.sql).
// TLS sans vérification du certificat (setInsecure) : chiffré, mais sans contrôle du serveur.
#pragma once
#include <Arduino.h>
#include <WiFi.h>

struct ResultatEnvoi {
  bool     tente = false;   // au moins un envoi a été essayé
  int      code = 0;        // code HTTP (201 = inséré) ou erreur négative de HTTPClient
  uint32_t quandMs = 0;     // millis() du dernier essai
  uint16_t nombre = 0;      // relevés publiés lors du dernier envoi réussi
  bool ok() const { return code == 201 || code == 200 || code == 204; }
};

class Reseau {
public:
  // Démarre (ou redémarre) la connexion avec ces identifiants ; ssid vide = Wi-Fi coupé.
  void demarrer(const String& ssid, const String& mdp);
  void arreter();
  void entretenir();                    // à appeler dans loop() : suit la connexion, lance le NTP

  bool actif() const { return _actif; }
  bool connecte() const { return _actif && WiFi.status() == WL_CONNECTED; }
  String ip() const { return connecte() ? WiFi.localIP().toString() : String(); }
  int rssi() const { return connecte() ? WiFi.RSSI() : 0; }
  const char* etat() const;             // "inactif", "connexion", "connecte"

  // Recherche des réseaux visibles (bloque 2 à 4 s) ; renvoie la ligne JSON {"wifi_reseaux":[...]}
  String scanner();

  // Insère un tableau JSON de relevés dans la table Supabase. Renvoie le code HTTP :
  // 201 = inséré ; 409 = au moins un relevé y est déjà (clé appareil + releve_id), rien n'est inséré.
  int envoyerSupabase(const String& url, const String& cle, const String& table, const String& lignesJson,
                      uint16_t nombre);

  const ResultatEnvoi& dernierEnvoi() const { return _dernier; }

private:
  bool _actif = false;
  bool _ntpLance = false;
  bool _etaitConnecte = false;
  uint32_t _depuisMs = 0;             // début de la tentative de connexion en cours
  String _ssid, _mdp;
  ResultatEnvoi _dernier;
};
