// CaravaneDemo — firmware de la carte caravane, pour Heltec V2 (ESP32) et V3 (ESP32-S3)
//
//  1. La carte démarre (mise sous tension, fin de veille, bouton USER ou retour du secteur).
//  2. Elle affiche le message d'accueil, la date-heure ("n/d" tant qu'elle n'a pas été
//     réglée), la température, l'humidité et la pression (carte CJMCU-8128).
//  3. Elle écoute le Bluetooth (BLE) : le téléphone règle le message, l'heure, les
//     temporisations et, côté administration, le mode de communication, le Wi-Fi,
//     Supabase et les broches. Un champ vide ne change rien.
//  4. Sur batterie sans Wi-Fi : veille profonde après 5 min sans activité, réveil 15 min après.
//     Sur secteur ou en mode Wi-Fi : pas de veille, la qualité d'air (CCS811) reste chaude,
//     l'écran s'éteint après 5 min sans activité (le bouton le rallume).
//  5. En mode Wi-Fi : mise à l'heure par Internet et envoi des mesures dans Supabase
//     à intervalle régulier (5 min par défaut).
//
// Carte : voir Config.h (V3 : « Heltec WiFi LoRa 32(V3) » ; V2 : « Heltec WiFi Kit 32 »
// avec le schéma de partition « Huge APP »).
// Bibliothèques à installer : U8g2 (olikraus), ArduinoJson (Benoit Blanchon, v7).

#include <ArduinoJson.h>
#include "driver/rtc_io.h"
#include "Config.h"
#include "Reglages.h"
#include "Horloge.h"
#include "Ecran.h"
#include "LiaisonBLE.h"
#include "Batterie.h"
#include "Environnement.h"
#include "Reseau.h"

Reglages reglages;
Ecran ecran;
LiaisonBLE ble;
Batterie batterie;
Environnement env;
Reseau reseau;

uint16_t vbatMv = 0;              // dernière tension LiPo lue (V3)
bool surSecteur = false;          // contact sec du relais 230 V fermé
bool redemarrageRequis = false;   // broches modifiées : prises en compte au prochain démarrage
uint8_t brocheSecteur = DEFAUT_SECTEUR;
String idAppareil;                // "caravane-a1b2c3", d'après l'adresse MAC

uint32_t derniereActiviteMs = 0;
uint32_t dernierAffichageMs = 0;
uint32_t dernierEnvoiMs = 0;
bool envoiFait = false;

bool lireSecteur() { return digitalRead(brocheSecteur) == LOW; }
bool capteurChaud() { return surSecteur || reglages.wifiActif(); }   // CCS811 actif en continu
bool peutDormir()   { return !surSecteur && !reglages.wifiActif(); } // veille profonde autorisée

static float arrondi1(float v) { return roundf(v * 10.0f) / 10.0f; }

// Mesures communes à l'état Bluetooth et à l'envoi Supabase
void ajouterMesures(JsonDocument& doc) {
  const Mesures& m = env.mesures();
  doc[CLE_SECTEUR] = surSecteur;
  if (Batterie::disponible()) doc[CLE_VBAT] = vbatMv;
  if (!isnan(m.temperatureC)) doc[CLE_TEMP]     = arrondi1(m.temperatureC);
  if (!isnan(m.humiditePct))  doc[CLE_HUMIDITE] = arrondi1(m.humiditePct);
  if (!isnan(m.pressionHpa))  doc[CLE_PRESSION] = arrondi1(m.pressionHpa);
  if (m.eco2Ppm) { doc[CLE_ECO2] = m.eco2Ppm; doc[CLE_TVOC] = m.tvocPpb; }
}

// État complet de la carte, renvoyé au téléphone à la connexion, après chaque commande
// et toutes les 10 s.
String etatJson() {
  JsonDocument doc;
  doc[CLE_MESSAGE]    = reglages.message();
  doc[CLE_HEURE]      = Horloge::valide() ? (int64_t)Horloge::epoch() : 0;
  doc[CLE_FUSEAU]     = reglages.fuseauMin();
  doc[CLE_INACTIVITE] = reglages.inactiviteS();
  doc[CLE_VEILLE]     = reglages.veilleS();
  ajouterMesures(doc);
  // Administration
  doc[CLE_CARTE]      = CARTE_NOM;
  doc["lora_dispo"]   = CARTE_LORA;
  doc["appareil"]     = idAppareil;
  doc[CLE_COMM]       = Reglages::nomComm(reglages.comm());
  doc[CLE_WIFI_SSID]  = reglages.wifiSsid();
  doc["wifi_mdp_def"] = reglages.wifiMdp().length() > 0;
  doc["wifi_etat"]    = reseau.etat();
  if (reseau.connecte()) { doc["wifi_ip"] = reseau.ip(); doc["wifi_rssi"] = reseau.rssi(); }
  doc[CLE_SB_URL]     = reglages.sbUrl();
  doc[CLE_SB_TABLE]   = reglages.sbTable();
  doc["sb_cle_def"]   = reglages.sbCle().length() > 0;
  doc[CLE_ENVOI]      = reglages.envoiS();
  const ResultatEnvoi& e = reseau.dernierEnvoi();
  if (e.tente) {
    JsonObject envoi = doc["dernier_envoi"].to<JsonObject>();
    envoi["code"] = e.code;
    envoi["age_s"] = (millis() - e.quandMs) / 1000;
  }
  JsonObject b = doc[CLE_BROCHES].to<JsonObject>();
  b["sda"] = reglages.capteursSda();
  b["scl"] = reglages.capteursScl();
  b["secteur"] = reglages.secteur();
  JsonObject d = doc["broches_defaut"].to<JsonObject>();
  d["sda"] = DEFAUT_CAPTEURS_SDA;
  d["scl"] = DEFAUT_CAPTEURS_SCL;
  d["secteur"] = DEFAUT_SECTEUR;
  if (redemarrageRequis) doc["redemarrage_requis"] = true;
  String sortie;
  serializeJson(doc, sortie);
  return sortie;
}

// Une ligne de la table Supabase (voir supabase/caravane_mesures.sql)
String ligneSupabase() {
  JsonDocument doc;
  doc["appareil"] = idAppareil;
  doc["carte"] = CARTE_NOM;
  doc["source"] = "wifi";
  if (Horloge::valide()) doc["mesure_le"] = Horloge::iso8601();
  ajouterMesures(doc);
  String sortie;
  serializeJson(doc, sortie);
  return sortie;
}

void envoyerMesures() {
  if (!reglages.supabasePret()) return;
  env.lire();
  vbatMv = batterie.lireMv();
  reseau.envoyerSupabase(reglages.sbUrl(), reglages.sbCle(), reglages.sbTable(), ligneSupabase());
  dernierEnvoiMs = millis();
  envoiFait = true;
}

// Démarre ou coupe le Wi-Fi et la qualité d'air selon le mode de communication
void appliquerComm() {
  if (reglages.wifiActif()) reseau.demarrer(reglages.wifiSsid(), reglages.wifiMdp());
  else reseau.arreter();
  env.activerQualiteAir(capteurChaud());
}

// Traite une ligne JSON venue du téléphone. Champ absent, vide ou nul = pas de changement.
void traiterCommande(const String& ligne) {
  if (ligne.length() == 0) return;
  JsonDocument doc;
  if (deserializeJson(doc, ligne)) {
    ble.envoyerLigne("{\"erreur\":\"json\"}");
    return;
  }
  if (doc[CLE_DEFAUT] | false) reglages.reinitialiser();
  reglages.definirMessage(doc[CLE_MESSAGE] | "");
  reglages.definirInactivite(doc[CLE_INACTIVITE] | 0UL);
  reglages.definirVeille(doc[CLE_VEILLE] | 0UL);
  reglages.definirEnvoi(doc[CLE_ENVOI] | 0UL);
  if (doc[CLE_FUSEAU].is<int>()) reglages.definirFuseau(doc[CLE_FUSEAU].as<int>());
  Horloge::regler(doc[CLE_HEURE] | (int64_t)0);

  bool wifiChange = reglages.definirWifi(doc[CLE_WIFI_SSID] | "", doc[CLE_WIFI_MDP] | "");
  bool commChange = reglages.definirComm(doc[CLE_COMM] | "");
  reglages.definirSupabase(doc[CLE_SB_URL] | "", doc[CLE_SB_CLE] | "", doc[CLE_SB_TABLE] | "");
  if (wifiChange && reglages.wifiActif()) reseau.arreter();      // reconnexion avec les nouveaux identifiants
  if (wifiChange || commChange) { appliquerComm(); envoiFait = false; }

  JsonObject broches = doc[CLE_BROCHES];
  if (!broches.isNull()) {
    for (const char* nom : { "sda", "scl", "secteur" }) {
      if (broches[nom].is<int>() && reglages.definirBroche(nom, broches[nom].as<int>())) redemarrageRequis = true;
    }
  }

  const String cmd = doc[CLE_CMD] | "";
  if (cmd == "scan_wifi") ble.envoyerLigne(reseau.scanner());
  else if (cmd == "envoyer") envoyerMesures();

  ble.envoyerLigne(etatJson());

  if (cmd == "redemarrer") {
    delay(500);
    ESP.restart();
  }
}

void rafraichirEcran() {
  const Mesures& m = env.mesures();
  char lEnv[24] = "capteurs absents";
  if (!isnan(m.temperatureC) || !isnan(m.pressionHpa)) {
    char t[8] = "--", h[6] = "--", p[8] = "--";
    if (!isnan(m.temperatureC)) snprintf(t, sizeof(t), "%.1fC", m.temperatureC);
    if (!isnan(m.humiditePct))  snprintf(h, sizeof(h), "%.0f%%", m.humiditePct);
    if (!isnan(m.pressionHpa))  snprintf(p, sizeof(p), "%.0fhPa", m.pressionHpa);
    snprintf(lEnv, sizeof(lEnv), "%s %s %s", t, h, p);
  }

  // Ligne 2 : suite du message, ou qualité d'air quand le capteur est chaud
  String msg = reglages.message();
  String ligne1 = msg.substring(0, 21), ligne2 = msg.length() > 21 ? msg.substring(21) : "";
  if (capteurChaud()) {
    char q[24];
    if (m.eco2Ppm) snprintf(q, sizeof(q), "CO2 %u COV %u", m.eco2Ppm, m.tvocPpb);
    else strlcpy(q, env.qualiteAirActive() ? "Air: chauffe..." : "Air: --", sizeof(q));
    ligne2 = q;
  }

  // Ligne d'état : Bluetooth, Wi-Fi, secteur ou décompte avant veille, LiPo
  String etat = ble.connecte() ? "BT" : "--";
  if (reglages.wifiActif()) etat += reseau.connecte() ? " WiFi" : " WiFi?";
  if (surSecteur) etat += " 230V";
  else if (peutDormir()) {
    uint32_t ecouleS = (millis() - derniereActiviteMs) / 1000;
    uint32_t resteS = ecouleS < reglages.inactiviteS() ? reglages.inactiviteS() - ecouleS : 0;
    char d[16];
    snprintf(d, sizeof(d), " %lu:%02lu", (unsigned long)(resteS / 60), (unsigned long)(resteS % 60));
    etat += d;
  }
  if (Batterie::disponible()) etat += " " + Batterie::texte(vbatMv);
  ecran.afficher(ligne1, ligne2, Horloge::texte(reglages.fuseauMin()), lEnv, etat);
}

void entrerEnVeille() {
  Serial.printf("Veille profonde pour %lu s\n", (unsigned long)reglages.veilleS());
  Serial.flush();
  digitalWrite(BROCHE_LED, LOW);
  reseau.arreter();
  ecran.eteindre();                                  // coupe aussi Vext, donc les capteurs
  esp_sleep_enable_timer_wakeup((uint64_t)reglages.veilleS() * 1000000ULL);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BROCHE_BOUTON, 0);   // bouton USER
  // Retour du secteur : le contact sec ramène la broche secteur à la masse
  if (rtc_gpio_is_valid_gpio((gpio_num_t)brocheSecteur)) {
#if CONFIG_IDF_TARGET_ESP32
    esp_sleep_enable_ext1_wakeup_io(1ULL << brocheSecteur, ESP_EXT1_WAKEUP_ALL_LOW);  // une seule broche
#else
    esp_sleep_enable_ext1_wakeup_io(1ULL << brocheSecteur, ESP_EXT1_WAKEUP_ANY_LOW);
#endif
    rtc_gpio_pullup_en((gpio_num_t)brocheSecteur);   // maintenu pendant la veille (domaine RTC actif grâce à ext0)
    rtc_gpio_pulldown_dis((gpio_num_t)brocheSecteur);
  }
  esp_deep_sleep_start();                            // au réveil : on repasse par setup()
}

// Bascule secteur / batterie (vérifiée chaque seconde)
void suivreSecteur() {
  bool s = lireSecteur();
  if (s == surSecteur) return;
  surSecteur = s;
  env.activerQualiteAir(capteurChaud());
  derniereActiviteMs = millis();                     // sur batterie, le décompte repart de zéro
  Serial.println(surSecteur ? "Secteur présent" : "Sur batterie");
}

void reveillerEcran() {
  derniereActiviteMs = millis();
  if (ecran.endormi()) { ecran.sommeil(false); rafraichirEcran(); }
}

void setup() {
  Serial.begin(115200);
  char id[20];
  snprintf(id, sizeof(id), "caravane-%06llx", (unsigned long long)(ESP.getEfuseMac() >> 24) & 0xFFFFFF);
  idAppareil = id;

  pinMode(BROCHE_BOUTON, INPUT_PULLUP);
  pinMode(BROCHE_LED, OUTPUT);
  digitalWrite(BROCHE_LED, LOW);                     // LED éteinte hors connexion Bluetooth
  reglages.charger();
  Serial.printf("Carte Heltec %s, %s\n", CARTE_NOM, idAppareil.c_str());

  brocheSecteur = reglages.secteur();
  if (rtc_gpio_is_valid_gpio((gpio_num_t)brocheSecteur)) rtc_gpio_deinit((gpio_num_t)brocheSecteur);
  pinMode(brocheSecteur, INPUT_PULLUP);

  uint8_t sda = reglages.capteursSda(), scl = reglages.capteursScl();
  bool busPartage = (sda == BROCHE_OLED_SDA && scl == BROCHE_OLED_SCL);
  ecran.demarrer(busPartage);                        // allume Vext : écran et capteurs
  env.demarrer(sda, scl, busPartage);
  env.balayer();
  batterie.demarrer();
  vbatMv = batterie.lireMv();
  surSecteur = lireSecteur();
  env.lire();
  ble.demarrer(BLE_NOM_APPAREIL);
  appliquerComm();                                   // Wi-Fi et qualité d'air selon le mode
  derniereActiviteMs = millis();
  rafraichirEcran();
  Serial.println(etatJson());
}

void loop() {
  ble.entretenir();
  reseau.entretenir();
  digitalWrite(BROCHE_LED, ble.connecte() ? HIGH : LOW);

  if (ble.activiteDepuisDernierAppel() || digitalRead(BROCHE_BOUTON) == LOW) reveillerEcran();

  // À chaque connexion, on pousse l'état courant (le téléphone peut aussi le demander avec "{}").
  if (ble.nouvelleConnexion()) { delay(1000); ble.envoyerLigne(etatJson()); }

  String ligne;
  while (ble.lireLigne(ligne)) traiterCommande(ligne);

  if (millis() - dernierAffichageMs >= 1000) {
    dernierAffichageMs = millis();
    suivreSecteur();
    static uint8_t compteur = 0;
    if (++compteur >= 10) {                          // toutes les 10 s
      compteur = 0;
      vbatMv = batterie.lireMv();
      env.lire();
      if (ble.connecte()) ble.envoyerLigne(etatJson());
    }
    rafraichirEcran();
  }

  // Envoi Supabase : dès la connexion Wi-Fi, puis à la période réglée
  if (reglages.wifiActif() && reseau.connecte() && reglages.supabasePret() &&
      (!envoiFait || millis() - dernierEnvoiMs >= reglages.envoiS() * 1000UL)) {
    envoyerMesures();
  }

  bool inactif = millis() - derniereActiviteMs >= reglages.inactiviteS() * 1000UL;
  if (inactif) {
    if (peutDormir()) entrerEnVeille();
    else ecran.sommeil(true);                        // reste éveillée : on éteint seulement l'écran
  }
  delay(20);
}
