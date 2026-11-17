# Modèle de données

## Supabase

Une ligne par relevé dans `caravane_releves` ; les mesures sont dans un objet JSON `valeurs`,
ce qui permet d'ajouter un capteur ou d'avoir des cartes équipées différemment sans toucher à la base.

| Colonne | Contenu |
|---|---|
| `appareil` | n° de série de la carte (MAC, 12 hexa) |
| `releve_id` | n° de relevé propre à la carte ; (`appareil`, `releve_id`) est unique : un renvoi est refusé (409) |
| `mesure_le` | heure du relevé sur la carte (null si inconnue) ; `recu_le` : arrivée dans Supabase |
| `nom`, `carte`, `dev_eui` | anciennes lignes seulement ; désormais dans `caravane_config` |
| `source` | `wifi` ou `lora` |
| `secteur` | caravane branchée sur le 230 V |
| `valeurs` | `{"temp_c":21.4,"hum_pct":55,"press_hpa":1013.2,"eco2_ppm":450,"tvoc_ppb":8,"vbat_mv":3950}` (clés présentes seulement si mesurées) |

- `caravane_grandeurs` : catalogue des clés (libellé, unité, ordre d'affichage). Une clé absente du catalogue est quand même stockée et affichée (libellé = code).
- `caravane_series` (vue) : format long `mesure_le / appareil / nom / grandeur / libelle / unite / valeur`, pour Grafana.
- `caravane_cartes` (vue) : dernière configuration connue de chaque carte (nom, carte, firmware, config complète).

### Mesures et configuration séparées

Les relevés ne portent que les mesures (corrigées par l'étalonnage) et leur contexte : n°, heure, secteur.
La configuration part à part dans `caravane_config` (`appareil`, `recu_le`, `evenement`, `config` en JSON), selon le principe du « jumeau numérique » des plateformes IoT :

| Événement | Quand |
|---|---|
| `demarrage` | à chaque démarrage (hors réveil éclair de la minuterie), avec la cause (`config.cause`) |
| `changement` | dès qu'un réglage publié change (nom, périodes, mode, étalonnage, broches) |
| `periodique` | toutes les 24 h sans changement : battement de cœur, prouve que la carte vit et resynchronise la base |

Contenu : nom, carte, DevEUI, version du firmware, mode de communication, périodes, fuseau, étalonnage complet, capteurs détectés, broches. Jamais de secret (mot de passe Wi-Fi, clé Supabase). La marque « à publier » est gardée en flash : un changement fait hors connexion part à la prochaine connexion. L'historique permet de retrouver l'étalonnage en vigueur à toute date (et donc la valeur brute : (valeur − décalage) / pente). Dans Grafana, les démarrages et changements sont marqués sur les courbes.

Grandeurs actuelles : `temp_c`, `hum_pct`, `press_hpa`, `eco2_ppm`, `tvoc_ppb`, `vbat_mv` ; prévues : `bat12_mv`, `bat12_ma` (batterie caravane).

## Trame LoRa (prévue, V3)

Même destination, mais trame binaire compacte : temps d'antenne limité (1 % en Europe, environ 30 s par jour et par appareil sur TTN). Le décodeur TTN (« payload formatter ») la transforme en `{"releve_id":…,"secteur":…,"valeurs":{…}}` et le webhook écrit dans `caravane_releves` avec `source = 'lora'`. L'identité de la carte vient de TTN (DevEUI).

Format envisagé (octets, petit-boutiste) :

| Octets | Champ |
|---|---|
| 1 | version du format |
| 2 | n° de relevé (16 bits bas, recomposé côté serveur) |
| 1 | drapeaux : bit 0 secteur |
| 1–2 | masque des grandeurs présentes (bit 7 = un 2ᵉ octet de masque suit) |
| … | valeurs présentes, dans l'ordre des bits |

| Bit | Grandeur | Codage |
|---|---|---|
| 0 | `temp_c` | int16, ×10 |
| 1 | `hum_pct` | uint8, ×2 (pas de 0,5 %) |
| 2 | `press_hpa` | uint16, (hPa − 500) ×10 |
| 3 | `eco2_ppm` | uint16 |
| 4 | `tvoc_ppb` | uint16 |
| 5 | `vbat_mv` | uint8, (mV − 2500) / 10 |
| 6 | `bat12_mv` | uint16 |

Relevé complet ≈ 15 octets. L'heure n'est pas transmise : TTN horodate la réception, et seul le dernier relevé est envoyé en LoRa.

La configuration passe sur un autre port LoRaWAN (FPort 2 ; mesures sur FPort 1), seulement au démarrage, sur changement et une fois par jour, en binaire compact (version firmware, période, étalonnages différents du défaut) ; le nom de la carte n'est pas transmis par radio (il est connu de la base par le Wi-Fi ou saisi dans TTN). Le décodeur TTN aiguille chaque port vers sa table.
