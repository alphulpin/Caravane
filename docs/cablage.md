# Câblage — Heltec WiFi LoRa 32 V3

Référence : `firmware/CaravaneDemo/Config.h`.

## Capteurs CJMCU-8128 (CCS811 + HDC1080 + BMP280)
Second bus I2C (`Wire1`, 100 kHz) : le premier (`Wire`, GPIO17/18) est pris par l'écran OLED.

| CJMCU-8128 | Heltec V3 | Connecteur |
|---|---|---|
| VCC | Ve (3,3 V commuté par GPIO36, coupé en veille) | J2 broche 3 ou 4 |
| GND | GND | J2 broche 1 |
| SDA | GPIO41 | J3 broche 15 |
| SCL | GPIO42 | J3 broche 16 |
| WAK | GND (CCS811 toujours éveillé) | — |
| INT, RST, ADD | non branchés | — |

Adresses attendues : HDC1080 `0x40`, CCS811 `0x5A` (ou `0x5B`), BMP280 `0x76`.
La qualité d'air (CCS811) n'est démarrée que sur secteur.

## Présence secteur
Contact sec du relais à bobine 230 V entre **GPIO7** (J2 broche 12) et GND.
Contact fermé = secteur présent. Non branché = batterie. Ce contact réveille aussi la carte en veille profonde.

## Mesures internes de la carte
- Tension LiPo : pont 390 kΩ / 100 kΩ sur GPIO1, activé par GPIO37 (actif haut sur V3.2, bas sur V3/V3.1 — détection automatique).
- Bouton USER/PRG : GPIO0 (réveil de la veille).

## Réutilisation de la carte de prototypage V2
Les connecteurs se superposent, mais le brochage diffère. Sur la carte V2 :
- CJMCU SDA → trou « 39 » de JP2 (= GPIO41 V3), SCL → trou « 38 » (= GPIO42 V3).
- Contact secteur → trou « 17 » de JP1 (= GPIO7 V3).
- Les rails « 3V3 » et « Vext » de la V2 tombent sur Ve et 3V3 de la V3 : Ve reste alors alimenté en permanence (les capteurs ne sont plus coupés pendant la veille).
