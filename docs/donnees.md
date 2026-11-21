# Modèle de données

## Supabase

Une ligne par relevé dans `caravane_releves` ; les mesures sont dans un objet JSON `valeurs`,
ce qui permet d'ajouter un capteur ou d'avoir des cartes équipées différemment sans toucher à la base.

| Colonne | Contenu |
|---|---|
| `appareil` | n° de série de la carte (MAC, 12 hexa) |
| `releve_id` | n° de relevé propre à la carte ; (`appareil`, `releve_id`) est unique : un renvoi est refusé (409) |
| `mesure_le` | heure du relevé sur la carte (null si inconnue) ; `recu_le` : arrivée dans Supabase |
| `horodatage` | colonne de temps de la série, calculée par la base : `mesure_le`, à défaut `recu_le` ; indexée avec `appareil` |
| `nom`, `carte`, `dev_eui` | anciennes lignes seulement ; désormais dans `caravane_config` |
| `source` | `wifi` ou `lora` |
| `secteur` | caravane branchée sur le 230 V |
| `valeurs` | `{"temp_c":21.4,"hum_pct":55,"press_hpa":1013.2,"eco2_ppm":450,"tvoc_ppb":8,"vbat_mv":3950}` (clés présentes seulement si mesurées) |

- `caravane_grandeurs` : catalogue des clés (libellé, unité, ordre d'affichage). Une clé absente du catalogue est quand même stockée et affichée (libellé = code).
- `caravane_series` (vue) : format long `horodatage / appareil / nom / grandeur / libelle / unite / valeur`, pour Grafana.
- `caravane_cartes` (vue) : dernière configuration connue de chaque carte (nom, carte, firmware, config complète).

### Séries temporelles : agrégats et conservation (`supabase/caravane_agregats.sql`)

PostgreSQL standard (TimescaleDB n'existe plus sur Supabase en Postgres 17) :

- **Index** sur (`appareil`, `horodatage`) : toute requête par carte et plage de temps l'utilise.
- **`caravane_agregats`** : par carte, grandeur et tranche (`heure`, ou `jour` = journée civile à Paris) : nombre de points, moyenne, minimum, maximum.
- **`caravane_agreger(depuis)`** recalcule les tranches qui contiennent un relevé *reçu* depuis cette date : un relevé publié en retard (gardé en mémoire par la carte) corrige bien sa tranche passée. Tâche `pg_cron` toutes les 15 min sur les dernières 24 h.
- **Conservation** : `caravane_purger(1 an)` supprime les relevés bruts de plus d'un an (tâche `pg_cron` mensuelle) ; les agrégats sont gardés indéfiniment (quelques Mo par an et par carte).
- **Grafana** choisit la résolution selon la plage affichée : relevés bruts jusqu'à 7 jours, moyennes horaires jusqu'à 90 jours, journalières au-delà (min et max en pointillés). Une interruption de transmission apparaît comme un trou dans la courbe (plus d'1 h sans relevé en vue brute, 3 h en horaire, 2 jours en journalier).

Suivi des tâches planifiées : `select jobname, schedule, active from cron.job where jobname like 'caravane-%';`

### Mesures et configuration séparées

Les relevés ne portent que les mesures (corrigées par l'étalonnage) et leur contexte : n°, heure, secteur.
La configuration part à part dans `caravane_config` (`appareil`, `recu_le`, `evenement`, `config` en JSON), selon le principe du « jumeau numérique » des plateformes IoT :

| Événement | Quand |
|---|---|
| `demarrage` | à chaque démarrage (hors réveil éclair de la minuterie), avec la cause (`config.cause`) |
| `changement` | dès qu'un réglage publié change (nom, périodes, mode, étalonnage, broches) |
| `periodique` | toutes les 24 h sans changement : battement de cœur, prouve que la carte vit et resynchronise la base |

Contenu : nom, carte, DevEUI, version du firmware, mode de communication, périodes, fuseau, étalonnage complet, capteurs détectés, broches. S'y ajoute un bloc `etat`, relevé au moment de l'envoi : tension LiPo de la carte (`vbat_mv`, V3), secteur ou batterie, signal Wi-Fi, température de la puce (`temp_puce_c`, V3 ; l'ESP32 de la V2 n'a plus ce capteur), durée de fonctionnement depuis le démarrage, relevés en attente. Ce bloc ne compte pas comme un changement de configuration : il suit simplement chaque envoi (démarrage, changement, battement quotidien). Jamais de secret (mot de passe Wi-Fi, clé Supabase). La marque « à publier » est gardée en flash : un changement fait hors connexion part à la prochaine connexion. L'historique permet de retrouver l'étalonnage en vigueur à toute date (et donc la valeur brute : (valeur − décalage) / pente). Dans Grafana, les démarrages et changements sont marqués sur les courbes.

Grandeurs actuelles : `temp_c`, `hum_pct`, `press_hpa`, `eco2_ppm`, `tvoc_ppb`, `vbat_mv` ; prévues : `bat12_mv`, `bat12_ma` (batterie caravane).

## Trame LoRa (V3)

Même destination, mais trame binaire compacte : temps d'antenne limité (1 % en Europe, environ 30 s par jour et par appareil sur TTN). Le décodeur TTN (`ttn/decodeur.js`) la transforme en `{"releve_id":…,"secteur":…,"valeurs":{…}}` ; le webhook TTN appelle la fonction Supabase `caravane_ttn` (`supabase/caravane_ttn.sql`), qui écrit dans `caravane_releves` avec `source = 'lora'`. L'identité de la carte vient du DevEUI (n° de série = DevEUI sans le `FFFE` central). La fonction ajoute aux valeurs la qualité radio de la meilleure passerelle : `lora_rssi` (dBm) et `lora_snr` (dB).

Port 1, octets en petit-boutiste (produit par `trameReleve()`, `Lora.cpp`) :

| Octets | Champ |
|---|---|
| 1 | version du format (1) |
| 4 | n° de relevé (uint32) |
| 1 | drapeaux : bit 0 secteur |
| 1 | masque des grandeurs présentes (bit 7 réservé : 2ᵉ octet de masque) |
| … | valeurs présentes, dans l'ordre des bits |

| Bit | Grandeur | Codage |
|---|---|---|
| 0 | `temp_c` | int16, ×10 |
| 1 | `hum_pct` | uint8, ×2 (pas de 0,5 %) |
| 2 | `press_hpa` | uint16, (hPa − 500) ×10 |
| 3 | `eco2_ppm` | uint16 |
| 4 | `tvoc_ppb` | uint16 |
| 5 | `vbat_mv` | uint8, (mV − 2500) / 10 |
| 6 | `bat12_mv` | uint16 (à venir) |

Relevé complet : 17 octets. L'heure n'est pas transmise : `mesure_le` = heure de réception TTN. Seul le dernier relevé est envoyé en LoRa ; en mode LoRa seul il est alors marqué publié, en mode Wi-Fi + LoRa le Wi-Fi publie tout (le doublon LoRa est ignoré par Supabase).

Connexion OTAA (LoRaWAN 1.0.3) ; la session est gardée en mémoire RTC pendant la veille profonde (pas de nouvelle connexion à chaque réveil) et les nonces de connexion en flash. Après 3 échecs de connexion, les tentatives s'espacent (un réveil sur 4, 8, puis 16).

### Configuration par LoRa (port 2)

Quand la configuration est à publier (démarrage, changement, battement d'environ 24 h) et que le Wi-Fi ne s'en charge pas, elle est **jointe au relevé** : même trame que le port 1, suivie de ce bloc, envoyée sur le port 2. Aucune émission supplémentaire.

| Octets | Champ |
|---|---|
| 1 | événement : 0 démarrage, 1 changement, 2 périodique |
| 1 | cause du démarrage : 0 autre, 1 mise sous tension, 2 redémarrage demandé, 3 plantage, 4 chien de garde, 5 baisse de tension, 6 réveil, 7 bouton reset |
| 3 | version du firmware (majeur, mineur, correctif) |
| 1 | mode de communication : 0 aucun, 1 Wi-Fi, 2 LoRa, 3 Wi-Fi + LoRa |
| 2 | période des relevés (min) |
| 2 | durée de veille (min) |
| 1 | température de la puce (°C, signé ; 127 = absente) |
| 1 | drapeaux : bit 0 journal série |
| 1 + n | longueur du nom puis nom (UTF-8, 21 octets au plus) |

Trame complète : 51 octets au plus, la limite des débits les plus lents (SF10 à SF12). La fonction `caravane_ttn` range le relevé dans `caravane_releves` et la configuration dans `caravane_config` (source `lora`), avec un bloc `etat` : tension LiPo, température de la puce, secteur, RSSI et SNR LoRa. L'étalonnage n'est pas transmis par radio.
