# Câblage

Le firmware reconnaît la carte à la compilation (puce ESP32-S3 → V3, ESP32 → V2). Référence : `firmware/CaravaneDemo/Config.h`. Les broches des capteurs et du contact secteur se règlent aussi depuis le téléphone (Administration > Broches).

## Heltec WiFi LoRa 32 V3 (ESP32-S3)

### Capteurs CJMCU-8128 (CCS811 + HDC1080 + BMP280)
Second bus I2C (`Wire1`, 100 kHz) : le premier (`Wire`, GPIO17/18) est pris par l'écran OLED.

| CJMCU-8128 | Heltec V3 | Connecteur |
|---|---|---|
| VCC | Ve (3,3 V commuté par GPIO36, coupé en veille) | J2 broche 3 ou 4 |
| GND | GND | J2 broche 1 |
| SDA | GPIO41 | J3 broche 15 |
| SCL | GPIO42 | J3 broche 16 |
| WAK | GND (CCS811 toujours éveillé) | — |
| INT, RST, ADD | non branchés | — |

### Contact secteur
Contact sec du relais 230 V entre **GPIO7** (J2 broche 12) et GND. Fermé = secteur présent ; non branché = batterie. Réveille la carte en veille profonde.

### Mesures internes
- LiPo : pont 390 kΩ / 100 kΩ sur GPIO1, activé par GPIO37 (actif haut sur V3.2, bas sur V3/V3.1 — détection automatique).
- Bouton USER/PRG : GPIO0 (réveil). LED blanche : GPIO35 (allumée pendant une connexion Bluetooth).

## Heltec WiFi Kit 32 V2 / WiFi LoRa 32 V2 (ESP32)

| Fonction | Broche |
|---|---|
| Écran OLED | SDA GPIO4, SCL GPIO15, RST GPIO16 |
| Vext (LOW = alimenté) | GPIO21 |
| LED blanche | GPIO25 |
| Bouton PRG | GPIO0 |
| Capteurs CJMCU (défaut) | **SDA GPIO4, SCL GPIO15 : même bus que l'écran** (câblage de la carte de prototypage V2), à 100 kHz |
| Contact secteur (défaut) | **GPIO32** (broche RTC : peut réveiller la carte) |
| Tension LiPo | non gérée |

Le contact secteur ne réveille la carte en veille que s'il est sur une broche RTC : GPIO0, 2, 4, 12 à 15, 25 à 27, 32 à 39 (34 à 39 sans résistance de tirage interne : à éviter ici).

### Carte de prototypage V2
Avec la V2, la carte de prototypage s'utilise telle quelle : la CJMCU branchée sur son connecteur I2C (SDA 04, SCL 15) est sur le bus de l'écran, ce que le firmware gère. Le contact secteur se branche sur le trou « 32 » de JP2.

### Réutiliser la carte de prototypage V2 avec la V3
Les connecteurs se superposent, mais le brochage diffère :
- CJMCU SDA → trou « 39 » de JP2 (= GPIO41 V3), SCL → trou « 38 » (= GPIO42 V3).
- Contact secteur → trou « 17 » de JP1 (= GPIO7 V3).
- Les rails « 3V3 » et « Vext » de la V2 tombent sur Ve et 3V3 de la V3 : Ve reste alors alimenté en permanence (les capteurs ne sont plus coupés pendant la veille).
