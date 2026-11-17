# Caravane
IoT for Caravan utilities management

Domotique de caravane sur carte Heltec (ESP32) : surveillance de l'alimentation (batterie / secteur), mesures d'environnement, réglage par Bluetooth depuis un téléphone, remontée des mesures vers Supabase par Wi-Fi (puis LoRaWAN via TTN), pilotage de la ventilation du frigo à venir.

## Contenu du dépôt

| Chemin | Rôle |
|---|---|
| `index.html` | Client web du téléphone (Web Bluetooth, Chrome Android), avec page Administration. Publié par GitHub Pages. |
| `firmware/CaravaneDemo/` | Croquis Arduino de la carte, commun aux Heltec V2 et V3 (ouvrir `CaravaneDemo.ino`). |
| `supabase/caravane_mesures.sql` | Création de la table des mesures et de ses droits (insertion seule avec la clé publique). |
| `docs/protocole-ble.md` | Protocole Bluetooth carte ↔ téléphone (champs JSON). |
| `docs/cablage.md` | Brochage des capteurs et du contact secteur, pour V2 et V3. |

À venir : décodeur de trames TTN et webhook vers Supabase (mode LoRa).

## Firmware

- IDE Arduino 2.x, paquet de cartes **esp32 d'Espressif 3.x**.
- Bibliothèques : **U8g2** (olikraus) et **ArduinoJson** (Benoit Blanchon, v7).
- Carte à choisir dans l'IDE :

| Carte | Menu Tools > Board | Partition Scheme |
|---|---|---|
| Heltec WiFi LoRa 32 V3 | Heltec WiFi LoRa 32(V3) | par défaut |
| Heltec WiFi Kit 32 V2 / WiFi LoRa 32 V2 | Heltec WiFi Kit 32 | **Huge APP (3MB No OTA/1MB SPIFFS)** — obligatoire : le programme dépasse 1,2 Mo |

Le modèle est reconnu à la compilation (ESP32-S3 → V3, ESP32 → V2) ; un même binaire ne peut pas servir aux deux puces.

### Modes de fonctionnement
- **Aucune communication** : sur batterie, veille profonde après 5 min sans activité, réveil toutes les 15 min, par le bouton ou au retour du secteur.
- **Secteur** : pas de veille, qualité d'air (CCS811) active ; l'écran s'éteint après 5 min sans activité.
- **Wi-Fi** : pas de veille, capteur de qualité d'air maintenu chaud, heure réglée par Internet (NTP), mesures écrites directement dans Supabase toutes les 5 min (réglable).
- **LoRa** : à venir (V3).

Tout se règle depuis la page **Administration** du client web : mode de communication, réseau Wi-Fi (recherche depuis la carte), Supabase, période d'envoi, broches.

## Supabase

1. Dans le projet Supabase : SQL Editor > coller `supabase/caravane_mesures.sql` > Run.
2. Project Settings > API : copier l'URL du projet et la clé publique (`anon` ou `publishable`).
3. Dans le client web, Administration > Supabase : coller l'URL et la clé, Enregistrer.

## Client téléphone

Page servie par GitHub Pages (Settings > Pages > branche `main`, dossier racine) :
`https://alphulpin.github.io/Caravane/`. Ouvrir dans Chrome sur Android, Bluetooth activé.
Un mode démonstration permet de l'essayer sans carte.
