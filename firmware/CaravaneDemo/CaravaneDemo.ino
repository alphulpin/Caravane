// CaravaneDemo — firmware de la carte caravane, pour Heltec V2 (ESP32) et V3 (ESP32-S3)
//
//  1. Démarrage (mise sous tension, bouton USER, retour du secteur) : l'écran affiche le nom
//     de la carte (« Hello world » s'il n'est pas défini), « bonjour! » et le n° de série,
//     la date-heure et les mesures (carte CJMCU-8128). Le Bluetooth (BLE) est à l'écoute.
//  2. Réveil par la minuterie, sur batterie sans Wi-Fi : relevé, enregistrement, retour
//     immédiat en veille (ni écran ni Bluetooth).
//  3. Chaque relevé est gardé en flash avec une marque « publié » (1000 relevés au plus).
//     En Wi-Fi, tous les relevés non publiés partent dans Supabase, par lots.
//     En LoRa (à venir), seul le dernier relevé sera envoyé.
//  4. Sur batterie sans Wi-Fi : veille profonde après 5 min sans activité, réveil 15 min après.
//     Sur secteur ou en Wi-Fi : pas de veille, relevé toutes les 15 min, qualité d'air (CCS811)
//     maintenue chaude ; après 5 min sans activité, l'écran ET le Bluetooth s'éteignent
//     (le bouton les rallume). Sur V2, couper le Bluetooth libère la mémoire du HTTPS.
//
// Carte : voir Config.h (V3 : « Heltec WiFi LoRa 32(V3) » ; V2 : « Heltec WiFi Kit 32 »
// avec le schéma de partition « Huge APP »).
// Bibliothèques à installer : U8g2 (olikraus), ArduinoJson (Benoit Blanchon, v7).

#include <ArduinoJson.h>
#include "driver/rtc_io.h"
#include "esp_bt.h"
#include "Config.h"
#include "Reglages.h"
#include "Horloge.h"
#include "Identite.h"
#include "Ecran.h"
#include "LiaisonBLE.h"
#include "Batterie.h"
#include "Environnement.h"
#include "Reseau.h"
#include "Releves.h"

Reglages reglages;
Ecran ecran;
LiaisonBLE ble;
Batterie batterie;
Environnement env;
Reseau reseau;
Releves releves;

String idCarte;                   // n° de série (adresse MAC), ex. "34B7DA5C1E20"
String devEui;                    // identifiant LoRaWAN (V3)
uint16_t vbatMv = 0;              // dernière tension LiPo lue (V3)
bool surSecteur = false;          // contact sec du relais 230 V fermé
bool redemarrageRequis = false;   // broches modifiées : prises en compte au prochain démarrage
uint8_t brocheSecteur = DEFAUT_SECTEUR;

uint32_t derniereActiviteMs = 0;
uint32_t dernierAffichageMs = 0;
uint32_t dernierReleveMs = 0;
uint32_t dernierEssaiPublicationMs = 0;

bool lireSecteur() { return digitalRead(brocheSecteur) == LOW; }
bool capteurChaud() { return surSecteur || reglages.wifiActif(); }   // CCS811 actif en continu
bool peutDormir()   { return !surSecteur && !reglages.wifiActif(); } // veille profonde autorisée

static float arrondi1(float v) { return roundf(v * 10.0f) / 10.0f; }

// Tension LiPo (V3), étalonnée ; 0 = pas de mesure
uint16_t lireVbat() {
  uint16_t brute = batterie.lireMv();
  if (brute == 0) return 0;
  return (uint16_t)constrain(lroundf(reglages.etalonnage().appliquer(G_VBAT, brute)), 1L, 65535L);
}

// ---------- Relevés ----------

// Relevés de ce démarrage pris avant que l'horloge soit réglée (le temps est perdu à chaque
// redémarrage) : on note l'instant millis() et on les date dès que l'heure arrive (Internet ou
// téléphone). Leur publication attend l'heure, 10 minutes au plus.
struct SansHeure { uint16_t index; uint32_t ms; };
SansHeure sansHeure[16];
uint8_t nbSansHeure = 0;
const uint32_t ATTENTE_HEURE_MS = 10UL * 60 * 1000;

void daterRelevesSansHeure() {
  if (!nbSansHeure || !Horloge::valide()) return;
  uint32_t maintenant = (uint32_t)Horloge::epoch();
  for (uint8_t i = 0; i < nbSansHeure; i++) {
    uint32_t h = maintenant - (millis() - sansHeure[i].ms) / 1000;
    bool ok = releves.dater(sansHeure[i].index, h);
    Serial.printf("Relevé (emplacement %u) daté après coup : %s\n", sansHeure[i].index, ok ? Horloge::iso8601(h).c_str() : "échec");
  }
  nbSansHeure = 0;
}

void enregistrerReleve() {
  env.lire();
  Serial.println(env.diagnostic());
  vbatMv = lireVbat();
  const Mesures& m = env.mesures();
  Releve r = {};
  r.numero      = reglages.prochainNumero();
  r.horodatage  = Horloge::valide() ? (uint32_t)Horloge::epoch() : 0;
  r.temp_c10    = isnan(m.temperatureC) ? INT16_MIN : (int16_t)lroundf(m.temperatureC * 10);
  r.hum_c10     = isnan(m.humiditePct) ? 0xFFFF : (uint16_t)lroundf(m.humiditePct * 10);
  r.press_hpa10 = isnan(m.pressionHpa) ? 0xFFFF : (uint16_t)lroundf(m.pressionHpa * 10);
  r.eco2_ppm    = m.eco2Ppm;
  r.tvoc_ppb    = m.tvocPpb;
  r.vbat_mv     = Batterie::disponible() ? vbatMv : 0;
  r.drapeaux    = surSecteur ? Releve::SECTEUR : 0;
  uint16_t index = 0;
  bool ok = releves.ajouter(r, &index);
  dernierReleveMs = millis();
  if (ok && !r.horodatage && nbSansHeure < 16) sansHeure[nbSansHeure++] = { index, dernierReleveMs };
  Serial.printf("Relevé n°%lu %s (%u à publier)\n", (unsigned long)r.numero, ok ? "enregistré" : "NON enregistré",
                releves.aPublier());
}

// Un relevé sous forme de ligne de la table Supabase caravane_releves : seulement les mesures
// et leur contexte (n°, heure, secteur) ; nom, carte, étalonnage… partent dans caravane_config.
// Les mesures sont dans l'objet « valeurs » (seulement celles qui existent ; un nouveau capteur =
// une clé de plus, sans toucher à la base).
void ajouterLigne(JsonArray lignes, const Releve& r) {
  JsonObject o = lignes.add<JsonObject>();
  o["appareil"]  = idCarte;
  o["releve_id"] = r.numero;
  o["source"]    = "wifi";
  if (r.horodatage) o["mesure_le"] = Horloge::iso8601(r.horodatage); else o["mesure_le"] = nullptr;
  o["secteur"]   = r.secteur();
  JsonObject v = o["valeurs"].to<JsonObject>();
  if (r.temp_c10 != INT16_MIN) v["temp_c"]    = r.temp_c10 / 10.0;
  if (r.hum_c10 != 0xFFFF)     v["hum_pct"]   = r.hum_c10 / 10.0;
  if (r.press_hpa10 != 0xFFFF) v["press_hpa"] = r.press_hpa10 / 10.0;
  if (r.eco2_ppm)            { v["eco2_ppm"]  = r.eco2_ppm; v["tvoc_ppb"] = r.tvoc_ppb; }
  if (r.vbat_mv)               v["vbat_mv"]   = r.vbat_mv;
}

// Mémoire suffisante pour une connexion HTTPS ? Sur l'ESP32 (V2), Bluetooth + Wi-Fi + TLS ne
// tiennent pas ensemble : on attend que le Bluetooth soit coupé. La V3 a assez de place.
bool memoirePourHttps() {
#if CONFIG_IDF_TARGET_ESP32
  return !ble.actif();               // V2 : jamais Bluetooth et HTTPS en même temps
#else
  return true;                       // V3 : assez de mémoire pour les deux
#endif
}

// Envoie lot[debut .. debut+n-1] à Supabase ; renvoie le code HTTP
int envoyerLot(const Releve* lot, size_t debut, size_t n) {
  JsonDocument doc;
  JsonArray lignes = doc.to<JsonArray>();
  for (size_t i = debut; i < debut + n; i++) ajouterLigne(lignes, lot[i]);
  String corps;
  serializeJson(doc, corps);
  return reseau.envoyerSupabase(reglages.sbUrl(), reglages.sbCle(), reglages.sbTable(), corps, n);
}

// Wi-Fi : publie les plus anciens relevés non publiés (un lot par appel)
void publierWifi() {
  static Releve lot[RELEVES_PAR_LOT];
  static uint16_t index[RELEVES_PAR_LOT];
  size_t n = releves.nonPublies(lot, index, RELEVES_PAR_LOT);
  dernierEssaiPublicationMs = millis();
  if (n == 0) return;
  int code = envoyerLot(lot, 0, n);
  if (reseau.dernierEnvoi().ok()) { releves.marquerPublies(index, n); return; }
  if (code == 409) {
    // Doublon : un relevé du lot est déjà dans Supabase (réponse perdue lors d'un envoi
    // précédent). On renvoie un par un ; « déjà là » vaut publication.
    size_t publies = 0;
    for (size_t i = 0; i < n; i++) {
      int c = envoyerLot(lot, i, 1);
      if (reseau.dernierEnvoi().ok() || c == 409) { releves.marquerPublies(&index[i], 1); publies++; }
      else break;
    }
    Serial.printf("Doublon : %u relevé(s) réglé(s) un par un\n", (unsigned)publies);
    return;
  }
  Serial.printf("Publication échouée (%d), nouvel essai dans 1 min\n", code);
}

// LoRa : seul le dernier relevé sera envoyé (à venir, après réception des passerelles).
// Il sera alors marqué publié ; les plus anciens restent à publier par le Wi-Fi.
void publierLora() {}

// ---------- Configuration publiée (jumeau numérique) ----------
// Envoyée à part des mesures, dans la table caravane_config : au démarrage, à chaque changement
// et une fois par jour (battement de cœur). Jamais de secret (mot de passe Wi-Fi, clé Supabase).
// La marque « à publier » est gardée en flash : un changement fait hors connexion part plus tard.

void ajouterEtalonnage(JsonObject e) {
  for (uint8_t g = 0; g < G_NOMBRE; g++) {
    JsonObject o = e[codeGrandeur(g)].to<JsonObject>();
    o["pente"]    = reglages.etalonnage()[g].pente;
    o["decalage"] = reglages.etalonnage()[g].decalage;
  }
}

// Contenu de la configuration (sert aussi d'empreinte pour détecter un changement)
void remplirConfig(JsonObject c) {
  c["nom"]          = reglages.nom();
  c["carte"]        = CARTE_NOM;
  if (CARTE_LORA) c["dev_eui"] = devEui;
  c["firmware"]     = FIRMWARE_VERSION;
  c["comm"]         = Reglages::nomComm(reglages.comm());
  c["releve_s"]     = reglages.releveS();
  c["inactivite_s"] = reglages.inactiviteS();
  c["veille_s"]     = reglages.veilleS();
  c["tz_min"]       = reglages.fuseauMin();
  ajouterEtalonnage(c["etalonnage"].to<JsonObject>());
  JsonObject cap = c["capteurs"].to<JsonObject>();
  cap["temp_hum"] = env.nomTH();
  cap["bmp280"]   = env.mesures().bmpOk;
  cap["ccs811"]   = env.mesures().ccsOk;
  JsonObject b = c["broches"].to<JsonObject>();
  b["sda"] = reglages.capteursSda();
  b["scl"] = reglages.capteursScl();
  b["secteur"] = reglages.secteur();
}

String empreinteConfig() {
  JsonDocument doc;
  remplirConfig(doc.to<JsonObject>());
  String s;
  serializeJson(doc, s);
  return s;
}

Preferences nvsConfig;                  // marque « configuration à publier », gardée en flash
String configEvenement;                 // "demarrage" | "changement" | "periodique" ; vide = rien à publier
uint32_t derniereConfigMs = 0;          // dernière publication réussie
uint32_t dernierEssaiConfigMs = 0;

void configAPublier(const char* evenement) {
  // Un démarrage pas encore publié reste un démarrage (il porte la cause du redémarrage)
  if (configEvenement == "demarrage" && strcmp(evenement, "demarrage") != 0) return;
  configEvenement = evenement;
  nvsConfig.putString("evt", configEvenement);
  Serial.printf("Configuration à publier (%s)\n", evenement);
}

const char* causeRedemarrage() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "mise sous tension";
    case ESP_RST_SW:        return "redémarrage demandé";
    case ESP_RST_PANIC:     return "plantage";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "chien de garde";
    case ESP_RST_BROWNOUT:  return "baisse de tension";
    case ESP_RST_DEEPSLEEP: return "réveil";
    case ESP_RST_EXT:       return "bouton reset";
    default:                return "autre";
  }
}

void publierConfig() {
  dernierEssaiConfigMs = millis();
  JsonDocument doc;
  JsonObject o = doc.to<JsonArray>().add<JsonObject>();
  o["appareil"]  = idCarte;
  o["source"]    = "wifi";
  o["evenement"] = configEvenement;
  JsonObject c = o["config"].to<JsonObject>();
  remplirConfig(c);
  if (configEvenement == "demarrage") c["cause"] = causeRedemarrage();
  String corps;
  serializeJson(doc, corps);
  reseau.envoyerSupabase(reglages.sbUrl(), reglages.sbCle(), SB_TABLE_CONFIG, corps, 1);
  if (!reseau.dernierEnvoi().ok()) return;
  Serial.printf("Configuration publiée (%s)\n", configEvenement.c_str());
  configEvenement = "";
  nvsConfig.remove("evt");
  derniereConfigMs = millis();
}

// ---------- État pour le téléphone ----------

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
  doc[CLE_NOM]        = reglages.nom();
  doc[CLE_HEURE]      = Horloge::valide() ? (int64_t)Horloge::epoch() : 0;
  doc[CLE_FUSEAU]     = reglages.fuseauMin();
  doc[CLE_INACTIVITE] = reglages.inactiviteS();
  doc[CLE_VEILLE]     = reglages.veilleS();
  doc[CLE_RELEVE]     = reglages.releveS();
  ajouterEtalonnage(doc[CLE_ETALONNAGE].to<JsonObject>());
  ajouterMesures(doc);
  // Identification et administration
  doc[CLE_CARTE]      = CARTE_NOM;
  doc[CLE_ID]         = idCarte;
  if (CARTE_LORA) doc[CLE_DEVEUI] = devEui;
  doc["lora_dispo"]   = CARTE_LORA;
  doc[CLE_COMM]       = Reglages::nomComm(reglages.comm());
  doc[CLE_WIFI_SSID]  = reglages.wifiSsid();
  doc["wifi_mdp_def"] = reglages.wifiMdp().length() > 0;
  doc["wifi_etat"]    = reseau.etat();
  if (reseau.connecte()) { doc["wifi_ip"] = reseau.ip(); doc["wifi_rssi"] = reseau.rssi(); }
  doc[CLE_SB_URL]     = reglages.sbUrl();
  doc[CLE_SB_TABLE]   = reglages.sbTable();
  doc["sb_cle_def"]   = reglages.sbCle().length() > 0;
  JsonObject j = doc["releves"].to<JsonObject>();
  j["nombre"]    = releves.nombre();
  j["a_publier"] = releves.aPublier();
  j["capacite"]  = releves.capacite();
  const ResultatEnvoi& e = reseau.dernierEnvoi();
  if (e.tente) {
    JsonObject envoi = doc["dernier_envoi"].to<JsonObject>();
    envoi["code"]   = e.code;
    envoi["nombre"] = e.nombre;
    envoi["age_s"]  = (millis() - e.quandMs) / 1000;
  }
  JsonObject b = doc[CLE_BROCHES].to<JsonObject>();
  b["sda"] = reglages.capteursSda();
  b["scl"] = reglages.capteursScl();
  b["secteur"] = reglages.secteur();
  JsonObject d = doc["broches_defaut"].to<JsonObject>();
  d["sda"] = DEFAUT_CAPTEURS_SDA;
  d["scl"] = DEFAUT_CAPTEURS_SCL;
  d["secteur"] = DEFAUT_SECTEUR;
  JsonObject c = doc["capteurs"].to<JsonObject>();
  c["temp_hum"] = env.nomTH();
  c["bmp280"] = env.mesures().bmpOk;
  c["ccs811"] = env.mesures().ccsOk;
  JsonArray i2c = c["i2c"].to<JsonArray>();
  const uint8_t* liste;
  uint8_t nb = env.adresses(liste);
  for (uint8_t i = 0; i < nb; i++) { char a[6]; snprintf(a, sizeof(a), "0x%02X", liste[i]); i2c.add(a); }
  if (redemarrageRequis) doc["redemarrage_requis"] = true;
  String sortie;
  serializeJson(doc, sortie);
  return sortie;
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
  const String avant = empreinteConfig();
  if (doc[CLE_DEFAUT] | false) reglages.reinitialiser();
  reglages.definirNom(doc[CLE_NOM] | "");
  reglages.definirInactivite(doc[CLE_INACTIVITE] | 0UL);
  reglages.definirVeille(doc[CLE_VEILLE] | 0UL);
  reglages.definirReleve(doc[CLE_RELEVE] | 0UL);
  // Étalonnage : {"temp_c":{"pente":1.02,"decalage":-1.5}} ; un champ absent garde sa valeur
  JsonObject etal = doc[CLE_ETALONNAGE];
  bool etalChange = false;
  for (JsonPair p : etal) {
    int g = grandeurDepuisCode(p.key().c_str());
    if (g < 0) continue;
    JsonObject v = p.value();
    const Etalon& e = reglages.etalonnage()[g];
    float pente = v["pente"].is<float>() ? v["pente"].as<float>() : e.pente;
    float decalage = v["decalage"].is<float>() ? v["decalage"].as<float>() : e.decalage;
    etalChange |= reglages.definirEtalon(g, pente, decalage);
  }
  if (etalChange) { env.lire(); vbatMv = lireVbat(); }   // valeurs corrigées tout de suite
  if (doc[CLE_FUSEAU].is<int>()) reglages.definirFuseau(doc[CLE_FUSEAU].as<int>());
  Horloge::regler(doc[CLE_HEURE] | (int64_t)0);

  bool wifiChange = reglages.definirWifi(doc[CLE_WIFI_SSID] | "", doc[CLE_WIFI_MDP] | "");
  bool commChange = reglages.definirComm(doc[CLE_COMM] | "");
  reglages.definirSupabase(doc[CLE_SB_URL] | "", doc[CLE_SB_CLE] | "", doc[CLE_SB_TABLE] | "");
  if (wifiChange && reglages.wifiActif()) reseau.arreter();      // reconnexion avec les nouveaux identifiants
  if (wifiChange || commChange) appliquerComm();

  JsonObject broches = doc[CLE_BROCHES];
  if (!broches.isNull()) {
    for (const char* nom : { "sda", "scl", "secteur" }) {
      if (broches[nom].is<int>() && reglages.definirBroche(nom, broches[nom].as<int>())) redemarrageRequis = true;
    }
  }

  const String cmd = doc[CLE_CMD] | "";
  if (cmd == "scan_wifi") ble.envoyerLigne(reseau.scanner());
  else if (cmd == "releve") { enregistrerReleve(); dernierEssaiPublicationMs = 0; }
  else if (cmd == "effacer_nom") reglages.effacerNom();
  else if (cmd == "effacer_releves") { releves.effacer(); nbSansHeure = 0; }

  if (empreinteConfig() != avant) configAPublier("changement");
  ble.envoyerLigne(etatJson());

  if (cmd == "redemarrer") {
    delay(500);
    ESP.restart();
  }
}

// ---------- Écran ----------

void rafraichirEcran() {
  const Mesures& m = env.mesures();

  // Ligne 4 : alterne toutes les 3 s entre les mesures, la qualité d'air et les relevés en attente
  String choix[3];
  uint8_t n = 0;
  char lEnv[24] = "capteurs absents";
  if (!isnan(m.temperatureC) || !isnan(m.pressionHpa)) {
    char t[8] = "--", h[6] = "--", p[8] = "--";
    if (!isnan(m.temperatureC)) snprintf(t, sizeof(t), "%.1fC", m.temperatureC);
    if (!isnan(m.humiditePct))  snprintf(h, sizeof(h), "%.0f%%", m.humiditePct);
    if (!isnan(m.pressionHpa))  snprintf(p, sizeof(p), "%.0fhPa", m.pressionHpa);
    snprintf(lEnv, sizeof(lEnv), "%s %s %s", t, h, p);
  }
  choix[n++] = lEnv;
  if (capteurChaud()) {
    char q[24];
    if (m.eco2Ppm) snprintf(q, sizeof(q), "CO2 %u COV %u", m.eco2Ppm, m.tvocPpb);
    else strlcpy(q, env.qualiteAirActive() ? "Air: chauffe..." : "Air: --", sizeof(q));
    choix[n++] = q;
  }
  if (releves.aPublier()) {
    char r[24];
    snprintf(r, sizeof(r), "A publier: %u", releves.aPublier());
    choix[n++] = r;
  }
  const String& ligne4 = choix[(millis() / 3000) % n];

  // Ligne d'état : Bluetooth, Wi-Fi, secteur ou décompte avant veille, LiPo
  String etat = ble.connecte() ? "BT" : (ble.actif() ? "bt" : "--");
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

  ecran.afficher(reglages.nomAffiche(), "bonjour! " + idCarte, Horloge::texte(reglages.fuseauMin()),
                 ligne4, etat);
}

// ---------- Veille ----------

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

// Réveil par la minuterie sur batterie : relevé, enregistrement, envoi LoRa, et on se rendort.
void cycleRapide(uint8_t sda, uint8_t scl, bool busPartage) {
  pinMode(BROCHE_VEXT, OUTPUT);
  digitalWrite(BROCHE_VEXT, LOW);                    // alimente les capteurs (l'écran reste éteint)
  delay(50);
  env.demarrer(sda, scl, busPartage);
  env.definirEtalonnage(&reglages.etalonnage());
  batterie.demarrer();
  releves.demarrer();
  enregistrerReleve();
  if (reglages.loraActif()) publierLora();
  entrerEnVeille();
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
  if (!ble.actif()) ble.demarrer(BLE_NOM_APPAREIL);
  if (ecran.endormi()) {                             // mesure fraîche pour l'écran rallumé
    vbatMv = lireVbat();
    env.lire();
    ecran.sommeil(false);
    rafraichirEcran();
  }
}

// ---------- Programme ----------

void setup() {
  Serial.begin(115200);
  idCarte = Identite::numeroSerie();
  devEui = Identite::devEui();
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  pinMode(BROCHE_BOUTON, INPUT_PULLUP);
  pinMode(BROCHE_LED, OUTPUT);
  digitalWrite(BROCHE_LED, LOW);                     // LED éteinte hors connexion Bluetooth
  reglages.charger();
  Serial.printf("Carte Heltec %s, n° %s, nom « %s »\n", CARTE_NOM, idCarte.c_str(), reglages.nomAffiche().c_str());

  brocheSecteur = reglages.secteur();
  if (rtc_gpio_is_valid_gpio((gpio_num_t)brocheSecteur)) rtc_gpio_deinit((gpio_num_t)brocheSecteur);
  pinMode(brocheSecteur, INPUT_PULLUP);
  surSecteur = lireSecteur();

  uint8_t sda = reglages.capteursSda(), scl = reglages.capteursScl();
  bool busPartage = (sda == BROCHE_OLED_SDA && scl == BROCHE_OLED_SCL);

  if (cause == ESP_SLEEP_WAKEUP_TIMER && peutDormir()) cycleRapide(sda, scl, busPartage);   // ne revient pas

  ecran.demarrer(busPartage);                        // allume Vext : écran et capteurs
  env.demarrer(sda, scl, busPartage);
  env.definirEtalonnage(&reglages.etalonnage());
  env.balayer();
  batterie.demarrer();
  releves.demarrer();
#if CONFIG_IDF_TARGET_ESP32
  // ESP32 (V2) : on n'utilise que le BLE. Rendre la mémoire réservée au Bluetooth « classique »
  // laisse assez de place au chiffrement HTTPS vers Supabase (sinon « Memory allocation failed »).
  esp_bt_mem_release(ESP_BT_MODE_CLASSIC_BT);
#endif
  ble.demarrer(BLE_NOM_APPAREIL);
  Serial.printf("Mémoire libre après Bluetooth : %u o (bloc max %u o)\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  appliquerComm();                                   // Wi-Fi et qualité d'air selon le mode
  nvsConfig.begin("caravane_cfg", false);
  configEvenement = nvsConfig.getString("evt", "");
  configAPublier("demarrage");
  enregistrerReleve();                               // un relevé à chaque démarrage
  derniereActiviteMs = millis();
  rafraichirEcran();
  Serial.println(etatJson());
}

void loop() {
  ble.entretenir();
  reseau.entretenir();
  digitalWrite(BROCHE_LED, ble.connecte() ? HIGH : LOW);

  if (digitalRead(BROCHE_BOUTON) == LOW || (ble.actif() && ble.activiteDepuisDernierAppel())) reveillerEcran();

  // À chaque connexion, on pousse l'état courant (le téléphone peut aussi le demander avec "{}").
  if (ble.nouvelleConnexion()) { delay(1000); ble.envoyerLigne(etatJson()); }

  String ligne;
  while (ble.lireLigne(ligne)) traiterCommande(ligne);

  if (millis() - dernierAffichageMs >= 1000) {
    dernierAffichageMs = millis();
    suivreSecteur();
    static uint8_t compteur = 0;
    // Mesures en direct toutes les 10 s seulement avec un téléphone connecté ; sinon les capteurs
    // restent alimentés (CCS811 chaud) mais ne sont lus qu'aux relevés, et l'écran montre le dernier.
    if (++compteur >= 10) {
      compteur = 0;
      if (ble.connecte()) {
        vbatMv = lireVbat();
        env.lire();
        Serial.println(env.diagnostic());
        ble.envoyerLigne(etatJson());
      }
    }
    rafraichirEcran();
  }

  // Relevé périodique tant que la carte reste éveillée
  if (millis() - dernierReleveMs >= reglages.releveS() * 1000UL) {
    enregistrerReleve();
    if (reglages.loraActif()) publierLora();
  }

  // Publication Wi-Fi des relevés en attente : lot suivant tout de suite si le précédent
  // est passé, sinon nouvel essai au bout d'une minute.
  daterRelevesSansHeure();
  if (configEvenement.isEmpty() && derniereConfigMs && millis() - derniereConfigMs >= CONFIG_PERIODE_S * 1000UL)
    configAPublier("periodique");
  bool publicationPossible = reglages.wifiActif() && reseau.connecte() && reglages.supabasePret() && memoirePourHttps();
  if (publicationPossible && configEvenement.length() &&
      (dernierEssaiConfigMs == 0 || millis() - dernierEssaiConfigMs >= 60000)) publierConfig();
  bool attendreHeure = nbSansHeure && millis() < ATTENTE_HEURE_MS;
  if (publicationPossible && releves.aPublier() && !attendreHeure) {
    uint32_t delai = reseau.dernierEnvoi().ok() ? 2000 : 60000;
    if (dernierEssaiPublicationMs == 0 || millis() - dernierEssaiPublicationMs >= delai) publierWifi();
  }

  bool inactif = millis() - derniereActiviteMs >= reglages.inactiviteS() * 1000UL;
  if (inactif) {
    if (peutDormir()) entrerEnVeille();
    else {                                           // reste éveillée : écran et Bluetooth coupés
      ecran.sommeil(true);
      ble.arreter();
    }
  }
  delay(20);
}
