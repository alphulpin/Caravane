# Caravane
IoT for Caravan utilities management

Domotique de caravane sur carte Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262) : surveillance de l'alimentation (batterie / secteur), pilotage de la ventilation du frigo, mesures d'environnement, réglage par Bluetooth depuis un téléphone, remontée LoRaWAN (TTN) vers Supabase.

## Contenu du dépôt

| Chemin | Rôle |
|---|---|
| `index.html` | Client web du téléphone (Web Bluetooth, Chrome Android). Publié par GitHub Pages. |
| `firmware/CaravaneDemo/` | Croquis Arduino de la carte (ouvrir `CaravaneDemo.ino` dans l'IDE Arduino). |
| `docs/protocole-ble.md` | Protocole Bluetooth carte ↔ téléphone (champs JSON). |
| `docs/cablage.md` | Brochage des capteurs et du contact secteur. |

À venir : `supabase/` (tables, webhook TTN) et le décodeur de trames TTN.

## Firmware

- IDE Arduino 2.x, paquet de cartes **esp32 d'Espressif 3.x**, carte « Heltec WiFi LoRa 32(V3) ».
- Bibliothèques : **U8g2** (olikraus) et **ArduinoJson** (Benoit Blanchon, v7).
- Pour travailler depuis ce dépôt : copier ou cloner `firmware/CaravaneDemo/` dans le dossier de croquis Arduino (le nom du dossier doit rester celui du `.ino`).

## Client téléphone

Page servie par GitHub Pages (Settings > Pages > branche `main`, dossier racine) :
`https://alphulpin.github.io/Caravane/`. Ouvrir dans Chrome sur Android, Bluetooth activé.
Un mode démonstration permet de l'essayer sans carte.
