# Protocole Bluetooth carte ↔ téléphone (CaravaneDemo)

Référence commune au firmware (`firmware/CaravaneDemo/Config.h`) et au client web du téléphone (`index.html`). Toute modification doit être faite des deux côtés.

## Liaison
- BLE (pas de Bluetooth classique sur ESP32-S3), compatible Web Bluetooth (Chrome Android, page servie en https).
- Nom annoncé : `Caravane`
- Service Nordic UART : `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
  - RX, téléphone → carte (write) : `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
  - TX, carte → téléphone (notify) : `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

## Format
Une ligne JSON terminée par `\n`, dans les deux sens. La carte découpe ses réponses en notifications de 20 octets : le client doit concaténer jusqu'au `\n`.

## Champs (téléphone → carte)
Un champ absent, vide (`""`) ou nul (`0`) ne modifie rien. Les durées et l'heure sont des nombres, pas des chaînes.

| Clé | Type | Rôle | Défaut codé en dur | Bornes |
|---|---|---|---|---|
| `msg` | texte | message d'accueil | `hello world` | 42 caractères |
| `heure` | nombre | heure du téléphone, epoch UTC en s ou en ms (`Date.now()`) | non réglée → `n/d` | — |
| `tz_min` | nombre | décalage local en minutes (`-new Date().getTimezoneOffset()`) | 60 | −720 à 840 |
| `inactivite_s` | nombre | inactivité avant veille profonde (sur batterie) | 300 | 30 à 3600 |
| `veille_s` | nombre | durée de la veille profonde | 900 | 60 à 86400 |
| `defaut` | booléen | `true` = retour aux valeurs codées en dur | — | — |

Exemple : `{"msg":"Bonjour","heure":1794813000,"tz_min":60,"inactivite_s":300,"veille_s":900}`

## Réponse (carte → téléphone)
À la connexion, après chaque commande et toutes les 10 s tant qu'un téléphone est connecté, la carte envoie son état complet : les mêmes clés (`heure` vaut 0 si non réglée), plus les mesures ci-dessous. Une mesure absente (capteur non détecté) n'apparaît pas.

| Clé | Type | Rôle |
|---|---|---|
| `vbat_mv` | nombre | tension de la LiPo branchée sur la carte, en mV (sans LiPo : sans signification) |
| `secteur` | booléen | `true` si le contact du relais 230 V est fermé |
| `temp_c` | nombre | température HDC1080, °C (1 décimale) |
| `hum_pct` | nombre | humidité relative HDC1080, % |
| `press_hpa` | nombre | pression BMP280, hPa |
| `eco2_ppm` | nombre | CO₂ équivalent CCS811, ppm — **sur secteur uniquement** |
| `tvoc_ppb` | nombre | COV totaux CCS811, ppb — **sur secteur uniquement** |

Envoyer `{}` renvoie l'état sans rien changer. JSON illisible : `{"erreur":"json"}`.
