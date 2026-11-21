# Passerelle LoRaWAN Dragino LPS8N et The Things Network (TTN)

Objectif : la passerelle, à la maison, relaie vers TTN les trames radio de la carte de la caravane
(environ 100 m). Rien n'est stocké sur la passerelle : elle transmet tout à TTN, qui décode puis
renverra les relevés vers Supabase (webhook, étape suivante).

```
Heltec V3 ──radio 868 MHz──> LPS8N ──Internet──> TTN (Europe 1) ──webhook──> Supabase ──> Grafana
```

Durée : 30 à 45 min. Il faut un PC (ou le téléphone) et un compte TTN.

## 1. Installer la passerelle

1. Choisir l'emplacement : en hauteur, près d'une fenêtre côté caravane, loin des masses métalliques
   (box, radiateur). La LPS8N est une passerelle d'intérieur ; à 100 m, ça suffit largement.
2. Brancher le câble réseau entre le port **WAN** de la LPS8N et la box (le plus simple et le plus fiable),
   puis l'alimentation USB-C.
3. Attendre 2 min. Voyants : rouge fixe = alimentée ; SYS **rouge** = pas d'Internet, **bleu clignotant** = Internet
   mais pas encore de serveur LoRaWAN, **bleu fixe** = connectée à TTN (but final).

## 2. Ouvrir l'interface de la passerelle

1. Sur le PC, se connecter au Wi-Fi **dragino-xxxxxx**, mot de passe **dragino+dragino**.
2. Ouvrir `http://10.130.1.1` (ajouter `:8000` si la page ne répond pas, selon la version du firmware).
3. Identifiant **root**, mot de passe **dragino**.
4. **Changer tout de suite le mot de passe** (System > General, ou System > Password) et le noter.
5. Page d'accueil : l'interface WAN (ETH) doit avoir une coche verte = Internet OK.

Sans câble possible : Network > WiFi (WiFi WAN Client) > choisir le Wi-Fi de la maison > mot de passe > Save & Apply.

Conseil : System > Firmware Upgrade, vérifier que le firmware est récent (sinon, image `…-squashfs-sysupgrade.bin`
depuis le site Dragino, décocher « Preserve Settings »).

## 3. Relever l'identifiant de la passerelle (Gateway EUI)

Menu **LoRaWAN > LoRaWAN** : noter le **Gateway ID**, 16 caractères hexadécimaux (ex. `A840411E96744154`).

Menu **LoRa > LoRa** : bande de fréquence **EU868** (863-870 MHz). Save & Apply.

## 4. Créer la passerelle dans TTN

1. Aller sur https://console.cloud.thethings.network, choisir le cluster **Europe 1 (eu1)**, se connecter
   (ou créer un compte The Things Network gratuit).
2. **Gateways > + Register gateway**.
3. **Gateway EUI** : celui de l'étape 3 > Confirm.
4. **Gateway ID** : par ex. `maison-lps8n` ; **Gateway name** : « Passerelle maison ».
5. **Frequency plan** : **Europe 863-870 MHz (SF9 for RX2 - recommended)**.
6. Laisser décochés « Require authenticated connection » et les cases de clés (mode simple, UDP).
7. **Register gateway**.

## 5. Brancher la passerelle sur TTN

Dans l'interface de la LPS8N, menu **LoRaWAN > LoRaWAN** (ou LoRaWAN > Semtech UDP selon le firmware) :

| Champ | Valeur |
|---|---|
| Service Provider | The Things Network V3 |
| Server Address | `eu1.cloud.thethings.network` |
| Uplink Port | `1700` |
| Downlink Port | `1700` |

**Save & Apply**. L'adresse doit correspondre au cluster choisi dans TTN (eu1), sinon les cartes ne pourront pas rejoindre le réseau.

Vérification, après 1 à 2 min :
- voyant SYS **bleu fixe** ;
- dans TTN, la passerelle passe à **Connected** (pastille verte), l'onglet **Live data** montre des « gateway status » réguliers ;
- côté passerelle : LogRead > Gateway Traffic.

Variante plus sûre, facultative : **LoRa Basics Station** (connexion chiffrée et authentifiée par clé API) ;
procédure Dragino « Notes for TTN » / documentation The Things Stack « Dragino LPS8N > LoRa Basics Station ».
Le mode UDP ci-dessus suffit pour démarrer.

## 6. Préparer l'application TTN de la caravane

1. **Applications > + Create application** : Application ID `caravane`, nom « Caravane ».
2. Dans l'application, **+ Register end device** :
   - Input method : **Enter end device specifics manually** ;
   - Frequency plan : **Europe 863-870 MHz (SF9 for RX2 - recommended)** ;
   - LoRaWAN version : **LoRaWAN Specification 1.0.3** ; Regional Parameters : **RP001 1.0.3 revision A** ;
   - **JoinEUI** (AppEUI) : `0000000000000000` > Confirm ;
   - **DevEUI** : celui de la carte, affiché dans le client téléphone, page Administration (« DevEUI »,
     construit à partir du n° de série : `xxxxxxFFFExxxxxx`) ;
   - **AppKey** : bouton **Generate**, puis la noter (elle sera saisie dans la carte par Bluetooth) ;
   - End device ID : par ex. `caravane-v3` > **Register end device**.
3. Garder l'onglet **Live data** de l'appareil ouvert : c'est là qu'apparaîtront le « join » puis les relevés.

## 7. Décodeur des trames

Dans l'application TTN : **Payload formatters > Uplink** > Formatter type **Custom Javascript formatter** >
coller tout le contenu de `ttn/decodeur.js` > **Save changes**.

Test possible sur la même page (« Test » : port 1, octets `01 70 11 01 00 00 3F D6 00 6F 0C 14 C2 01 08 00 95`) :
on doit lire température 21,4 °C, humidité 55,5 %, pression 1013,2 hPa…

## 8. Webhook vers Supabase

1. Dans Supabase, SQL Editor : lancer `supabase/caravane_ttn.sql` (une seule fois ; relançable).
2. Dans TTN : **Integrations > Webhooks > + Add webhook > Custom webhook** :

| Champ | Valeur |
|---|---|
| Webhook ID | `supabase` |
| Webhook format | JSON |
| Base URL | `https://<ref-du-projet>.supabase.co/rest/v1/rpc` |
| Additional headers | `apikey` = clé **publishable** ; `Authorization` = `Bearer ` + la même clé |
| Enabled event types | cocher seulement **Uplink message**, chemin `/caravane_ttn` |

3. **Add webhook**. À chaque trame, TTN appelle la fonction, qui range le relevé dans `caravane_releves` (source `lora`).
   En cas d'échec, TTN l'indique sur la page du webhook (pastille rouge, message d'erreur).

## 9. Carte V3

1. Arduino IDE : installer la bibliothèque **RadioLib** de Jan Gromeš, **version 7.8.1** (Gestionnaire de bibliothèques,
   choisir la version dans la liste ; la 8.0 a changé son interface).
2. Téléverser le firmware sur la V3 (carte « Heltec WiFi LoRa 32(V3) »).
3. Sur le téléphone, page Administration :
   - section LoRaWAN : JoinEUI `0000000000000000`, **AppKey** copiée depuis TTN (appareil > Activation information) ;
   - Mode de communication : **LoRa** (ou Wi-Fi + LoRa) > Enregistrer.
4. La carte se connecte au réseau au relevé suivant (bouton « Relevé maintenant » pour ne pas attendre).
   Dans TTN, onglet **Live data** de l'appareil : « Accept join request », puis « Forward uplink data message »
   avec les valeurs décodées. Sur le téléphone, la ligne « LoRa (TTN) » passe à « connecté ».
5. Journal série : `LoRa : connecté en … ms`, puis `LoRa : trame n°… envoyée`.

Sur batterie (mode LoRa seul, pas de secteur), la carte dort entre deux relevés et garde sa session LoRaWAN :
pas de nouvelle connexion à chaque réveil.

## En cas de souci

| Symptôme | Piste |
|---|---|
| SYS rouge | pas d'Internet : câble WAN, ou Wi-Fi client mal configuré |
| SYS bleu clignotant | Internet OK mais TTN non joint : adresse serveur, ports 1700, EUI identique dans TTN |
| Pas de « join » dans Live data | AppKey ou DevEUI différents de TTN ; carte trop loin ; journal : `pas de réponse` |
| « DevNonce already used » dans TTN | la carte a été réinitialisée : appareil TTN > Settings > Network layer > Advanced MAC settings > cocher « Resets join nonces » le temps d’une connexion |
| Webhook en erreur | clé Supabase, chemin `/caravane_ttn`, script `caravane_ttn.sql` non lancé |
| Passerelle « Disconnected » dans TTN | mauvais cluster (eu1), EUI mal saisi, pare-feu de la box bloquant l'UDP sortant 1700 |
| Plus d'accès à l'interface | se reconnecter au Wi-Fi dragino-xxxxxx (10.130.1.1) ; en dernier recours, bouton de réinitialisation |
