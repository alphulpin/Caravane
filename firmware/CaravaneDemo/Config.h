// ==== Caravane =======================================================================
// Config.h — firmware CaravaneDemo, version 0.10.2 — dernière modification : 2026-11-21
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Config.h — brochage par carte, valeurs par défaut et protocole Bluetooth.
// Tout ce qui doit être aligné avec le client web du téléphone est regroupé ici.
//
// La carte est reconnue à la compilation, d'après la puce choisie dans l'IDE :
//   - ESP32-S3 -> Heltec WiFi LoRa 32 V3  (carte « Heltec WiFi LoRa 32(V3) »)
//   - ESP32    -> Heltec WiFi Kit 32 V2 / WiFi LoRa 32 V2 (carte « Heltec WiFi Kit 32 »,
//                 schéma de partition « Huge APP (3MB No OTA/1MB SPIFFS) »)
// Un même binaire ne peut pas tourner sur les deux puces : on recompile pour chaque carte.
// Les broches des capteurs et du contact secteur restent réglables depuis le téléphone.
#pragma once
#include <Arduino.h>

#if CONFIG_IDF_TARGET_ESP32S3
// ---------- Heltec WiFi LoRa 32 V3 (ESP32-S3) ----------
#define CARTE_NOM "V3"
constexpr bool    CARTE_LORA      = true;
constexpr uint8_t BROCHE_OLED_SDA = 17;
constexpr uint8_t BROCHE_OLED_SCL = 18;
constexpr uint8_t BROCHE_OLED_RST = 21;
constexpr uint8_t BROCHE_VEXT     = 36;  // LOW = Ve alimenté (écran, capteurs)
constexpr uint8_t BROCHE_LED      = 35;  // LED blanche
constexpr int8_t  BROCHE_VBAT     = 1;   // mesure LiPo (pont 390k/100k)
constexpr int8_t  BROCHE_VBAT_CTRL = 37; // mise en service du pont
// Défauts réglables : CJMCU-8128 sur un second bus, contact secteur
constexpr uint8_t DEFAUT_CAPTEURS_SDA = 41;   // J3 broche 15
constexpr uint8_t DEFAUT_CAPTEURS_SCL = 42;   // J3 broche 16
constexpr uint8_t DEFAUT_SECTEUR      = 7;    // J2 broche 12
#else
// ---------- Heltec WiFi Kit 32 V2 / WiFi LoRa 32 V2 (ESP32) ----------
#define CARTE_NOM "V2"
constexpr bool    CARTE_LORA      = false;   // LoRa non géré sur cette carte
constexpr uint8_t BROCHE_OLED_SDA = 4;
constexpr uint8_t BROCHE_OLED_SCL = 15;
constexpr uint8_t BROCHE_OLED_RST = 16;
constexpr uint8_t BROCHE_VEXT     = 21;  // LOW = Vext alimenté
constexpr uint8_t BROCHE_LED      = 25;  // LED blanche
constexpr int8_t  BROCHE_VBAT     = -1;  // mesure LiPo non gérée sur V2
constexpr int8_t  BROCHE_VBAT_CTRL = -1;
// Défauts réglables : sur la carte de prototypage V2, la CJMCU partage le bus de l'écran
constexpr uint8_t DEFAUT_CAPTEURS_SDA = 4;
constexpr uint8_t DEFAUT_CAPTEURS_SCL = 15;
constexpr uint8_t DEFAUT_SECTEUR      = 32;   // broche RTC : peut réveiller la carte
#endif

constexpr uint8_t BROCHE_BOUTON = 0;     // bouton USER/PRG, actif à l'état bas (les deux cartes)

// Câblage CJMCU-8128 : VCC -> Ve/Vext (ou 3V3), GND -> GND, SDA/SCL -> broches ci-dessus,
// WAK -> GND (pont conservé), INT/RST/ADD non branchés.
// Contact secteur : contact sec du relais 230 V entre la broche secteur et GND (fermé = secteur).

// ---------- Valeurs par défaut (codées en dur) ----------
#define NOM_DEFAUT "Hello world"          // affiché tant que la carte n'a pas de nom
#define SB_TABLE_DEFAUT "caravane_releves"
#define SB_TABLE_CONFIG "caravane_config"   // configuration publiée au démarrage, à chaque changement et chaque jour
#define FIRMWARE_VERSION "0.10.2"
constexpr uint32_t CONFIG_PERIODE_S = 24UL * 3600;  // battement de cœur de la configuration
constexpr uint32_t INACTIVITE_DEFAUT_S = 5 * 60;   // avant mise en veille profonde
constexpr uint32_t VEILLE_DEFAUT_S     = 15 * 60;  // durée de la veille profonde
constexpr uint32_t RELEVE_DEFAUT_S     = 15 * 60;  // période des relevés quand la carte reste éveillée
constexpr int16_t  FUSEAU_DEFAUT_MIN   = 60;       // décalage heure locale / UTC (hiver France)

// Bornes acceptées pour les réglages venant du téléphone
constexpr uint32_t INACTIVITE_MIN_S = 30,  INACTIVITE_MAX_S = 3600;
constexpr uint32_t VEILLE_MIN_S     = 60,  VEILLE_MAX_S     = 86400;
constexpr uint32_t RELEVE_MIN_S     = 60,  RELEVE_MAX_S     = 86400;
constexpr size_t   NOM_LONGUEUR_MAX = 21;          // une ligne d'écran
constexpr uint16_t RELEVES_CAPACITE = 1000;        // relevés gardés en flash (≈10 jours à 4 par heure)
constexpr uint8_t  RELEVES_PAR_LOT  = 10;          // relevés par envoi Supabase

// Modes de communication
enum class ModeComm : uint8_t { Aucun = 0, Wifi = 1, Lora = 2, WifiLora = 3 };

// ---------- Protocole Bluetooth (BLE, compatible Web Bluetooth) ----------
// Service "Nordic UART" : le téléphone écrit sur RX, la carte notifie sur TX.
#define BLE_NOM_APPAREIL "Caravane"
#define BLE_UUID_SERVICE "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_UUID_RX      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // téléphone -> carte (write)
#define BLE_UUID_TX      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // carte -> téléphone (notify)

// Échanges : une ligne JSON terminée par '\n', dans les deux sens.
// Un champ absent, vide ("") ou nul (0) ne modifie rien (sauf les broches, voir plus bas).
#define CLE_NOM        "nom"           // nom de la carte (21 caractères), affiché et publié
#define CLE_HEURE      "heure"         // heure du téléphone, epoch UTC (secondes ou millisecondes)
#define CLE_FUSEAU     "tz_min"        // décalage local en minutes (= -getTimezoneOffset() en JS)
#define CLE_INACTIVITE "inactivite_s"  // délai d'inactivité avant veille, en secondes
#define CLE_VEILLE     "veille_s"      // durée de veille profonde, en secondes
#define CLE_JOURNAL    "journal"       // true/false : messages de suivi sur le port série (défaut true)
#define CLE_ETALONNAGE "etalonnage"    // {"temp_c":{"pente":1,"decalage":-1.5},...} : valeur = pente × brute + décalage
#define CLE_RELEVE     "releve_s"      // période des relevés quand la carte reste éveillée, en secondes
#define CLE_DEFAUT     "defaut"        // true = temporisations d'origine (nom, Wi-Fi, Supabase, broches conservés)
// Administration
#define CLE_COMM       "comm"          // "aucun" | "wifi" | "lora" | "wifi_lora"
#define CLE_WIFI_SSID  "wifi_ssid"     // nom du réseau Wi-Fi
#define CLE_WIFI_MDP   "wifi_mdp"      // mot de passe (jamais renvoyé)
#define CLE_SB_URL     "sb_url"        // https://<projet>.supabase.co
#define CLE_SB_CLE     "sb_cle"        // clé API Supabase (jamais renvoyée)
#define CLE_SB_TABLE   "sb_table"      // table de destination
#define CLE_LORA_JOINEUI "lora_joineui"  // JoinEUI (AppEUI) LoRaWAN, 16 hexa (défaut 0000000000000000)
#define CLE_LORA_APPKEY  "lora_appkey"   // AppKey LoRaWAN, 32 hexa (jamais renvoyée)
#define CLE_BROCHES    "broches"       // {"sda":41,"scl":42,"secteur":7} ; -1 = défaut de la carte
#define CLE_CMD        "cmd"           // "scan_wifi" | "releve" | "effacer_nom" | "effacer_releves" | "redemarrer"
// Réponses seulement
#define CLE_CARTE      "carte"         // "V2" ou "V3"
#define CLE_ID         "id_carte"      // n° de série : adresse MAC d'usine, 12 chiffres hexadécimaux
#define CLE_DEVEUI     "dev_eui"       // identifiant LoRaWAN (V3), tiré de l'adresse MAC
#define CLE_VBAT       "vbat_mv"       // tension LiPo de la carte, en mV (V3 seulement)
#define CLE_SECTEUR    "secteur"       // true si secteur présent
#define CLE_TEMP       "temp_c"        // température HDC1080, °C
#define CLE_HUMIDITE   "hum_pct"       // humidité HDC1080, %
#define CLE_PRESSION   "press_hpa"     // pression BMP280, hPa
#define CLE_ECO2       "eco2_ppm"      // CO2 équivalent CCS811 (capteur actif seulement)
#define CLE_TVOC       "tvoc_ppb"      // COV totaux CCS811 (capteur actif seulement)
