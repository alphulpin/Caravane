# Caravane
IoT for Caravan utilities management

Domotique de caravane sur carte Heltec (ESP32) : surveillance de l'alimentation (batterie / secteur), mesures d'environnement, réglage par Bluetooth depuis un téléphone, remontée des mesures vers Supabase par Wi-Fi (puis LoRaWAN via TTN), pilotage de la ventilation du frigo à venir.

## Contenu du dépôt

| Chemin | Rôle |
|---|---|
| `index.html` | Client web du téléphone (Web Bluetooth, Chrome Android), avec page Administration. Publié par GitHub Pages. |
| `firmware/CaravaneDemo/` | Croquis Arduino de la carte, commun aux Heltec V2 et V3 (ouvrir `CaravaneDemo.ino`). |
| `supabase/caravane_releves.sql` | Base des relevés : table `caravane_releves` (en-tête + valeurs en JSON), catalogue `caravane_grandeurs`, vue `caravane_series` (carte / grandeur / date / valeur), droits (insertion seule avec la clé publique). |
| `supabase/grafana_lecture.sql` | Compte en lecture seule pour Grafana. |
| `grafana/caravane-releves.json` | Tableau de bord Grafana à importer (voir `docs/grafana.md`). |
| `docs/protocole-ble.md` | Protocole Bluetooth carte ↔ téléphone (champs JSON). |
| `docs/cablage.md` | Brochage des capteurs et du contact secteur, pour V2 et V3. |
| `docs/grafana.md` | Visualisation des relevés dans Grafana Cloud, pas à pas. |
| `docs/donnees.md` | Modèle de données et trame LoRa prévue. |
| `outils/gen_grafana.py` | Génère le tableau de bord Grafana. |

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

### Identification
- **Nom** : choisi depuis le téléphone, gardé en flash, affiché en première ligne de l'écran (« Hello world » tant qu'il n'est pas défini) et publié dans Supabase.
- **N° de série** : l'adresse MAC d'usine de la puce (12 chiffres hexadécimaux), affichée après « bonjour! » ; c'est la clé `appareil` dans Supabase.
- **DevEUI** (V3, LoRaWAN) : l'adresse MAC avec `FFFE` au milieu, à déclarer dans TTN.

### Modes de fonctionnement
- **Relevés** : à chaque démarrage et à chaque réveil, puis toutes les 15 min (réglable) si la carte reste éveillée. Ils sont gardés en flash (1000 au plus, ≈ 10 jours à 4 par heure) avec une marque « publié ».
- **Aucune communication** : sur batterie, veille profonde après 5 min sans activité ; réveil toutes les 15 min pour un relevé éclair (sans écran ni Bluetooth), par le bouton ou au retour du secteur.
- **Secteur** : pas de veille, qualité d'air (CCS811) active ; l'écran s'éteint après 5 min sans activité.
- **Wi-Fi** : pas de veille, capteur de qualité d'air maintenu chaud, heure réglée par Internet (NTP), publication dans Supabase de **tous les relevés non publiés**, y compris ceux accumulés hors connexion.
- **LoRa** : à venir (V3) ; seul le dernier relevé sera envoyé.

Tout se règle depuis le client web : nom de la carte (Réglages) ; mode de communication, réseau Wi-Fi (recherche depuis la carte), Supabase, période des relevés, broches (Administration).

## Supabase

1. Dans le projet Supabase : SQL Editor > coller `supabase/caravane_releves.sql` > Run. (Relançable ; reprend les lignes de l'ancienne table `caravane_mesures` si elle existe.)
2. Project Settings > API : copier l'URL du projet et la clé publique (`anon` ou `publishable`).
3. Dans le client web, Administration > Supabase : coller l'URL et la clé, Enregistrer.
4. Visualisation : voir `docs/grafana.md`.

## Client téléphone

Page servie par GitHub Pages (Settings > Pages > branche `main`, dossier racine) :
`https://alphulpin.github.io/Caravane/`. Ouvrir dans Chrome sur Android, Bluetooth activé.
Un mode démonstration permet de l'essayer sans carte.
