# Protocole Bluetooth carte ↔ téléphone (CaravaneDemo)

Référence commune au firmware (`firmware/CaravaneDemo/Config.h`) et au client web du téléphone (`index.html`). Toute modification doit être faite des deux côtés.

## Liaison
- BLE (compatible Web Bluetooth : Chrome Android, page servie en https).
- Nom annoncé : `Caravane`
- Service Nordic UART : `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
  - RX, téléphone → carte (write) : `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
  - TX, carte → téléphone (notify) : `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

## Format
Une ligne JSON terminée par `\n`, dans les deux sens (1024 octets au plus). La carte découpe ses réponses en notifications de 20 octets : le client doit concaténer jusqu'au `\n`.

## Réglages (téléphone → carte)
Un champ absent, vide (`""`) ou nul (`0`) ne modifie rien. Les durées et l'heure sont des nombres, pas des chaînes.

| Clé | Type | Rôle | Défaut codé en dur | Bornes |
|---|---|---|---|---|
| `msg` | texte | message d'accueil | `hello world` | 42 caractères |
| `heure` | nombre | heure du téléphone, epoch UTC en s ou en ms (`Date.now()`) | non réglée → `n/d` | — |
| `tz_min` | nombre | décalage local en minutes (`-new Date().getTimezoneOffset()`) | 60 | −720 à 840 |
| `inactivite_s` | nombre | inactivité avant veille profonde (ou avant extinction de l'écran si la carte reste éveillée) | 300 | 30 à 3600 |
| `veille_s` | nombre | durée de la veille profonde | 900 | 60 à 86400 |
| `defaut` | booléen | `true` = message et temporisations d'origine (Wi-Fi, Supabase et broches conservés) | — | — |

## Administration (téléphone → carte)

| Clé | Type | Rôle | Défaut |
|---|---|---|---|
| `comm` | texte | `aucun`, `wifi`, `lora`, `wifi_lora` (LoRa refusé sur V2) | `aucun` |
| `wifi_ssid` | texte | nom du réseau Wi-Fi | — |
| `wifi_mdp` | texte | mot de passe Wi-Fi (jamais renvoyé) | — |
| `sb_url` | texte | `https://<projet>.supabase.co` | — |
| `sb_cle` | texte | clé API publique Supabase (jamais renvoyée) | — |
| `sb_table` | texte | table de destination | `caravane_mesures` |
| `envoi_s` | nombre | période d'envoi Wi-Fi vers Supabase, en s (30 à 86400) | 300 |
| `broches` | objet | `{"sda":41,"scl":42,"secteur":7}` ; **une clé absente = inchangée, `-1` = défaut de la carte** (0 est une broche valide). Pris en compte au redémarrage. | selon la carte |
| `cmd` | texte | `scan_wifi` (recherche des réseaux), `envoyer` (envoi Supabase immédiat), `redemarrer` | — |

Exemples :
- `{"comm":"wifi","wifi_ssid":"Maison","wifi_mdp":"…"}`
- `{"sb_url":"https://xxxx.supabase.co","sb_cle":"…","envoi_s":300}`
- `{"broches":{"secteur":13},"cmd":"redemarrer"}`

Réponse à `scan_wifi` (ligne séparée, avant l'état) : `{"wifi_reseaux":[{"ssid":"Maison","rssi":-52,"ouvert":false},…]}` — 15 réseaux au plus, du plus fort au plus faible.

## État (carte → téléphone)
À la connexion, après chaque commande et toutes les 10 s tant qu'un téléphone est connecté, la carte envoie son état complet : les clés de réglage ci-dessus (`heure` vaut 0 si non réglée), plus :

| Clé | Type | Rôle |
|---|---|---|
| `secteur` | booléen | `true` si le contact du relais 230 V est fermé |
| `vbat_mv` | nombre | tension LiPo de la carte, en mV — **V3 seulement** |
| `temp_c`, `hum_pct` | nombre | température (°C) et humidité (%) HDC1080 |
| `press_hpa` | nombre | pression BMP280, hPa |
| `eco2_ppm`, `tvoc_ppb` | nombre | CCS811 — seulement quand le capteur est chaud (secteur ou mode Wi-Fi) |
| `carte` | texte | `V2` ou `V3` (fixé à la compilation) |
| `lora_dispo` | booléen | la carte gère LoRa |
| `appareil` | texte | identifiant `caravane-xxxxxx` (adresse MAC), repris dans Supabase |
| `comm`, `wifi_ssid`, `sb_url`, `sb_table`, `envoi_s` | | réglages d'administration en cours |
| `wifi_mdp_def`, `sb_cle_def` | booléen | un mot de passe / une clé est enregistré |
| `wifi_etat` | texte | `inactif`, `connexion`, `connecte` ; avec `wifi_ip` et `wifi_rssi` (dBm) une fois connecté |
| `dernier_envoi` | objet | `{"code":201,"age_s":42}` — code HTTP (201 = inséré), `-100` = Wi-Fi absent |
| `broches`, `broches_defaut` | objet | broches en service et broches par défaut de la carte |
| `redemarrage_requis` | booléen | des broches ont changé, redémarrage nécessaire |

Une mesure absente (capteur non détecté) n'apparaît pas. Envoyer `{}` renvoie l'état sans rien changer. JSON illisible : `{"erreur":"json"}`.

## Sécurité
Le Bluetooth n'est pas appairé : toute personne à portée peut se connecter et modifier les réglages. Le mot de passe Wi-Fi et la clé Supabase transitent en clair sur le lien BLE (mais ne sont jamais relus). Utiliser une clé Supabase publique limitée à l'insertion (voir `supabase/caravane_mesures.sql`).
