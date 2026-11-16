// CaravaneDemo — démo de base pour Heltec WiFi LoRa 32 V3
//
//  1. La carte démarre (mise sous tension, fin de veille, bouton USER ou retour du secteur).
//  2. Elle affiche le message d'accueil, la date-heure ("n/d" tant qu'elle n'a pas été
//     réglée), la température, l'humidité et la pression (carte CJMCU-8128).
//  3. Elle écoute le Bluetooth (BLE) : le téléphone peut changer le message, régler
//     l'heure et modifier les deux temporisations. Un champ vide ne change rien.
//  4. Sur batterie : après 5 min sans activité, veille profonde ; réveil au bout de 15 min.
//     Sur secteur : pas de veille, et la qualité d'air (CCS811) tourne en permanence.
//
// Carte : "Heltec WiFi LoRa 32(V3)" (paquet esp32 d'Espressif, version 3.x)
// Bibliothèques à installer : U8g2 (olikraus), ArduinoJson (Benoit Blanchon, v7).
// Câblage : voir Config.h.

#include <ArduinoJson.h>
#include "driver/rtc_io.h"
#include "Config.h"
#include "Reglages.h"
#include "Horloge.h"
#include "Ecran.h"
#include "LiaisonBLE.h"
#include "Batterie.h"
#include "Environnement.h"

Reglages reglages;
Ecran ecran;
LiaisonBLE ble;
Batterie batterie;
Environnement env;

uint16_t vbatMv = 0;              // dernière tension LiPo lue
bool surSecteur = false;          // contact sec du relais 230 V fermé

uint32_t derniereActiviteMs = 0;
uint32_t dernierAffichageMs = 0;

bool lireSecteur() { return digitalRead(BROCHE_SECTEUR) == LOW; }

static float arrondi1(float v) { return roundf(v * 10.0f) / 10.0f; }

// État complet de la carte, renvoyé au téléphone à la connexion et après chaque commande.
String etatJson() {
  const Mesures& m = env.mesures();
  JsonDocument doc;
  doc[CLE_MESSAGE]    = reglages.message();
  doc[CLE_HEURE]      = Horloge::valide() ? (int64_t)Horloge::epoch() : 0;
  doc[CLE_FUSEAU]     = reglages.fuseauMin();
  doc[CLE_INACTIVITE] = reglages.inactiviteS();
  doc[CLE_VEILLE]     = reglages.veilleS();
  doc[CLE_VBAT]       = vbatMv;
  doc[CLE_SECTEUR]    = surSecteur;
  if (!isnan(m.temperatureC)) doc[CLE_TEMP]     = arrondi1(m.temperatureC);
  if (!isnan(m.humiditePct))  doc[CLE_HUMIDITE] = arrondi1(m.humiditePct);
  if (!isnan(m.pressionHpa))  doc[CLE_PRESSION] = arrondi1(m.pressionHpa);
  if (m.eco2Ppm) { doc[CLE_ECO2] = m.eco2Ppm; doc[CLE_TVOC] = m.tvocPpb; }
  String sortie;
  serializeJson(doc, sortie);
  return sortie;
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
  if (doc[CLE_FUSEAU].is<int>()) reglages.definirFuseau(doc[CLE_FUSEAU].as<int>());
  Horloge::regler(doc[CLE_HEURE] | (int64_t)0);
  ble.envoyerLigne(etatJson());
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

  // Ligne 2 : suite du message, ou qualité d'air sur secteur
  String msg = reglages.message();
  String ligne1 = msg.substring(0, 21), ligne2 = msg.length() > 21 ? msg.substring(21) : "";
  if (surSecteur) {
    char q[24];
    if (m.eco2Ppm) snprintf(q, sizeof(q), "CO2 %u COV %u", m.eco2Ppm, m.tvocPpb);
    else strlcpy(q, env.qualiteAirActive() ? "Air: chauffe..." : "Air: --", sizeof(q));
    ligne2 = q;
  }

  char etat[28];
  if (surSecteur) {
    snprintf(etat, sizeof(etat), "%s Bat %s 230V", ble.connecte() ? "BT ok" : "BT --",
             Batterie::texte(vbatMv).c_str());
  } else {
    uint32_t ecouleS = (millis() - derniereActiviteMs) / 1000;
    uint32_t resteS = ecouleS < reglages.inactiviteS() ? reglages.inactiviteS() - ecouleS : 0;
    snprintf(etat, sizeof(etat), "%s Bat %s %lu:%02lu", ble.connecte() ? "BT ok" : "BT --",
             Batterie::texte(vbatMv).c_str(),
             (unsigned long)(resteS / 60), (unsigned long)(resteS % 60));
  }
  ecran.afficher(ligne1, ligne2, Horloge::texte(reglages.fuseauMin()), lEnv, etat);
}

void entrerEnVeille() {
  Serial.printf("Veille profonde pour %lu s\n", (unsigned long)reglages.veilleS());
  Serial.flush();
  digitalWrite(BROCHE_LED, LOW);
  ecran.eteindre();                                  // coupe aussi Ve, donc les capteurs
  esp_sleep_enable_timer_wakeup((uint64_t)reglages.veilleS() * 1000000ULL);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BROCHE_BOUTON, 0);   // bouton USER
  // Retour du secteur : le contact sec ramène BROCHE_SECTEUR à la masse
  esp_sleep_enable_ext1_wakeup_io(1ULL << BROCHE_SECTEUR, ESP_EXT1_WAKEUP_ANY_LOW);
  rtc_gpio_pullup_en((gpio_num_t)BROCHE_SECTEUR);    // maintenu pendant la veille (domaine RTC actif grâce à ext0)
  rtc_gpio_pulldown_dis((gpio_num_t)BROCHE_SECTEUR);
  esp_deep_sleep_start();                            // au réveil : on repasse par setup()
}

// Bascule secteur / batterie (vérifiée chaque seconde)
void suivreSecteur() {
  bool s = lireSecteur();
  if (s == surSecteur) return;
  surSecteur = s;
  env.activerQualiteAir(surSecteur);
  derniereActiviteMs = millis();                     // sur batterie, le décompte repart de 5 min
  Serial.println(surSecteur ? "Secteur présent" : "Sur batterie");
}

void setup() {
  Serial.begin(115200);
  pinMode(BROCHE_BOUTON, INPUT_PULLUP);
  pinMode(BROCHE_LED, OUTPUT);
  digitalWrite(BROCHE_LED, LOW);                     // LED éteinte hors connexion Bluetooth
  rtc_gpio_deinit((gpio_num_t)BROCHE_SECTEUR);       // rend la broche au GPIO normal après la veille
  pinMode(BROCHE_SECTEUR, INPUT_PULLUP);
  reglages.charger();
  ecran.demarrer();                                  // allume Ve : écran et capteurs
  env.demarrer(BROCHE_CAPTEURS_SDA, BROCHE_CAPTEURS_SCL);
  env.balayer();
  batterie.demarrer();
  vbatMv = batterie.lireMv();
  surSecteur = lireSecteur();
  env.activerQualiteAir(surSecteur);
  env.lire();
  ble.demarrer(BLE_NOM_APPAREIL);
  derniereActiviteMs = millis();
  rafraichirEcran();
  Serial.println(etatJson());
}

void loop() {
  ble.entretenir();
  digitalWrite(BROCHE_LED, ble.connecte() ? HIGH : LOW);

  if (ble.activiteDepuisDernierAppel() || digitalRead(BROCHE_BOUTON) == LOW) derniereActiviteMs = millis();

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

  if (!surSecteur && millis() - derniereActiviteMs >= reglages.inactiviteS() * 1000UL) entrerEnVeille();
  delay(20);
}
