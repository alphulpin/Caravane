// Config.h — brochage Heltec WiFi LoRa 32 V3, valeurs par défaut et protocole Bluetooth.
// Tout ce qui doit être aligné avec le client web du téléphone est regroupé ici.
#pragma once
#include <Arduino.h>

// ---------- Brochage Heltec WiFi LoRa 32 V3 ----------
constexpr uint8_t BROCHE_OLED_SDA = 17;
constexpr uint8_t BROCHE_OLED_SCL = 18;
constexpr uint8_t BROCHE_OLED_RST = 21;
constexpr uint8_t BROCHE_VEXT     = 36;  // LOW = écran alimenté
constexpr uint8_t BROCHE_BOUTON   = 0;   // bouton USER (PRG), actif à l'état bas
constexpr uint8_t BROCHE_LED      = 35;  // LED blanche : allumée seulement quand un téléphone est connecté

// Capteurs CJMCU-8128 (second bus I2C, alimentés par Ve = coupé pendant la veille)
//   CJMCU VCC -> Ve (J2 broche 3 ou 4)     CJMCU GND -> GND
//   CJMCU SDA -> GPIO41 (J3 broche 15)     CJMCU SCL -> GPIO42 (J3 broche 16)
//   CJMCU WAK -> GND (pont conservé)       INT, RST, ADD : non branchés
constexpr uint8_t BROCHE_CAPTEURS_SDA = 41;
constexpr uint8_t BROCHE_CAPTEURS_SCL = 42;

// Présence secteur : contact sec du relais 230 V entre GPIO7 (J2 broche 12) et GND.
// Contact fermé = secteur présent. Non branché = batterie.
constexpr uint8_t BROCHE_SECTEUR = 7;

// ---------- Valeurs par défaut (codées en dur) ----------
#define MESSAGE_DEFAUT "hello world"
constexpr uint32_t INACTIVITE_DEFAUT_S = 5 * 60;   // avant mise en veille profonde
constexpr uint32_t VEILLE_DEFAUT_S     = 15 * 60;  // durée de la veille profonde
constexpr int16_t  FUSEAU_DEFAUT_MIN   = 60;       // décalage heure locale / UTC (hiver France)

// Bornes acceptées pour les réglages venant du téléphone
constexpr uint32_t INACTIVITE_MIN_S = 30,  INACTIVITE_MAX_S = 3600;
constexpr uint32_t VEILLE_MIN_S     = 60,  VEILLE_MAX_S     = 86400;
constexpr size_t   MESSAGE_LONGUEUR_MAX = 42;      // 2 lignes de 21 caractères

// ---------- Protocole Bluetooth (BLE, compatible Web Bluetooth) ----------
// Service "Nordic UART" : le téléphone écrit sur RX, la carte notifie sur TX.
#define BLE_NOM_APPAREIL "Caravane"
#define BLE_UUID_SERVICE "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_UUID_RX      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // téléphone -> carte (write)
#define BLE_UUID_TX      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // carte -> téléphone (notify)

// Échanges : une ligne JSON terminée par '\n', dans les deux sens.
// Un champ absent, vide ("") ou nul (0) ne modifie rien.
#define CLE_MESSAGE    "msg"           // texte d'accueil
#define CLE_HEURE      "heure"         // heure du téléphone, epoch UTC (secondes ou millisecondes)
#define CLE_FUSEAU     "tz_min"        // décalage local en minutes (= -getTimezoneOffset() en JS)
#define CLE_INACTIVITE "inactivite_s"  // délai d'inactivité avant veille, en secondes
#define CLE_VEILLE     "veille_s"      // durée de veille profonde, en secondes
#define CLE_VBAT       "vbat_mv"       // (réponse seulement) tension LiPo de la carte, en mV
#define CLE_SECTEUR    "secteur"       // (réponse) true si secteur présent
#define CLE_TEMP       "temp_c"        // (réponse) température HDC1080, °C
#define CLE_HUMIDITE   "hum_pct"       // (réponse) humidité HDC1080, %
#define CLE_PRESSION   "press_hpa"     // (réponse) pression BMP280, hPa
#define CLE_ECO2       "eco2_ppm"      // (réponse, secteur seulement) CO2 équivalent CCS811
#define CLE_TVOC       "tvoc_ppb"      // (réponse, secteur seulement) COV totaux CCS811
#define CLE_DEFAUT     "defaut"        // true = retour aux valeurs codées en dur
