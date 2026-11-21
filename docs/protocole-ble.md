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
| `nom` | texte | nom de la carte, conservé en flash ; affiché à la place de « Hello world » et publié dans Supabase | aucun (écran : `Hello world`) | 21 caractères |
| `heure` | nombre | heure du téléphone, epoch UTC en s ou en ms (`Date.now()`) | non réglée → `n/d` | — |
| `tz_min` | nombre | décalage local en minutes (`-new Date().getTimezoneOffset()`) | 60 | −720 à 840 |
| `inactivite_s` | nombre | inactivité avant veille profonde (ou avant extinction de l'écran si la carte reste éveillée) | 300 | 30 à 3600 |
| `veille_s` | nombre | durée de la veille profonde | 900 | 60 à 86400 |
| `lora_joineui` | texte | JoinEUI LoRaWAN (16 hexa) | `0000000000000000` | |
| `lora_appkey` | texte | AppKey LoRaWAN (32 hexa) ; jamais renvoyée (l'état indique `lora_cle_def`) ; un changement relance la connexion au réseau | — | |
| `journal` | booléen | messages de suivi sur le port série (moniteur de l'IDE) ; conservé par `defaut` | true | |
| `etalonnage` | objet | par grandeur `{"pente":p,"decalage":d}` : valeur = p × brute + d ; seules les grandeurs et clés envoyées changent ; conservé par `defaut` | pente 1, décalage 0 | pente 0,5 à 2 ; décalage ±10 °C, ±20 %, ±50 hPa, ±500 ppm/ppb/mV |
| `releve_s` | nombre | période des relevés quand la carte reste éveillée (secteur ou Wi-Fi) ; sur batterie, un relevé à chaque réveil | 900 | 60 à 86400 |
| `defaut` | booléen | `true` = temporisations d'origine (nom, Wi-Fi, Supabase et broches conservés) | — | — |

## Administration (téléphone → carte)

| Clé | Type | Rôle | Défaut |
|---|---|---|---|
| `comm` | texte | `aucun`, `wifi`, `lora`, `wifi_lora` (LoRa refusé sur V2) | `aucun` |
| `wifi_ssid` | texte | nom du réseau Wi-Fi | — |
| `wifi_mdp` | texte | mot de passe Wi-Fi (jamais renvoyé) | — |
| `sb_url` | texte | `https://<projet>.supabase.co` | — |
| `sb_cle` | texte | clé API publique Supabase (jamais renvoyée) | — |
| `sb_table` | texte | table de destination | `caravane_releves` |
| `broches` | objet | `{"sda":41,"scl":42,"secteur":7}` ; **une clé absente = inchangée, `-1` = défaut de la carte** (0 est une broche valide). Pris en compte au redémarrage. | selon la carte |
| `cmd` | texte | `scan_wifi` (recherche des réseaux), `releve` (relevé immédiat, publié dans la foulée en Wi-Fi), `effacer_nom` (retour à « Hello world »), `effacer_releves` (vide le journal), `redemarrer` | — |

Exemples :
- `{"comm":"wifi","wifi_ssid":"Maison","wifi_mdp":"…"}`
- `{"nom":"Caravane Bob"}`
- `{"sb_url":"https://xxxx.supabase.co","sb_cle":"…","releve_s":900}`
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
| `id_carte` | texte | n° de série : adresse MAC d'usine, 12 chiffres hexadécimaux (colonne `appareil` dans Supabase) |
| `dev_eui` | texte | identifiant LoRaWAN, V3 seulement : l'adresse MAC avec `FFFE` au milieu |
| `comm`, `wifi_ssid`, `sb_url`, `sb_table` | | réglages d'administration en cours |
| `releves` | objet | `{"nombre":37,"a_publier":5,"capacite":1000}` : journal des relevés en flash |
| `wifi_mdp_def`, `sb_cle_def` | booléen | un mot de passe / une clé est enregistré |
| `wifi_etat` | texte | `inactif`, `connexion`, `connecte` ; avec `wifi_ip` et `wifi_rssi` (dBm) une fois connecté |
| `dernier_envoi` | objet | `{"code":201,"nombre":20,"age_s":42}` — code HTTP (201 = inséré, 409 = doublon), `-100` = Wi-Fi absent ; `nombre` = relevés publiés |
| `broches`, `broches_defaut` | objet | broches en service et broches par défaut de la carte |
| `redemarrage_requis` | booléen | des broches ont changé, redémarrage nécessaire |

Une mesure absente (capteur non détecté) n'apparaît pas. Envoyer `{}` renvoie l'état sans rien changer. JSON illisible : `{"erreur":"json"}`.

## Relevés et publication
- Chaque relevé est enregistré en flash (fichier LittleFS `/releves.bin`, 24 octets par relevé, 1000 au plus ; au-delà, le plus ancien est remplacé) avec une marque « publié ».
- Moments des relevés : à chaque démarrage, à chaque réveil de la minuterie (la carte se rendort aussitôt, sans écran ni Bluetooth), puis toutes les `releve_s` secondes tant qu'elle reste éveillée, et sur `cmd:"releve"`.
- **Wi-Fi** : tous les relevés non publiés partent vers Supabase, du plus ancien au plus récent, par lots de 20. Un lot refusé pour doublon (409) est renvoyé ligne par ligne ; un relevé déjà présent compte comme publié.
- **LoRa** (à venir) : seul le dernier relevé est envoyé et marqué publié ; les plus anciens attendent le Wi-Fi.
- Chaque relevé porte un numéro propre à la carte (`releve_id`, conservé en flash, jamais réutilisé) : Supabase refuse d'enregistrer deux fois le même couple (appareil, releve_id).

## Sécurité
Le Bluetooth n'est pas appairé : toute personne à portée peut se connecter et modifier les réglages. Le mot de passe Wi-Fi et la clé Supabase transitent en clair sur le lien BLE (mais ne sont jamais relus). Utiliser une clé Supabase publique limitée à l'insertion (voir `supabase/caravane_releves.sql`).

## Étalonnage

Exemple : `{"etalonnage":{"temp_c":{"decalage":-1.5},"hum_pct":{"pente":1.03,"decalage":-2}}}`.
Grandeurs : `temp_c`, `hum_pct`, `press_hpa`, `eco2_ppm`, `tvoc_ppb`, `vbat_mv`. L'état renvoyé
contient toujours l'objet `etalonnage` complet. Les relevés sont enregistrés corrigés ; tout
changement d'étalonnage est publié dans la table `caravane_config` (voir `donnees.md`).

## État LoRa (V3)

L'état contient `lora_joineui`, `lora_cle_def` (AppKey saisie), `lora_etat` (« connecté », « pas encore connecté »,
« non connecté (code) », « clé manquante ») et, après un premier envoi, `lora_dernier` :
`{"code":0,"age_s":12,"toa_ms":62,"trames":12,"rssi":-71,"snr":8.5}` (rssi/snr seulement si le réseau a répondu).
