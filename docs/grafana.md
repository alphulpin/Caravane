# Visualiser les relevés dans Grafana Cloud

Grafana lit directement la base Supabase avec un compte en lecture seule. Le tableau de bord prêt à importer est `grafana/caravane-releves.json`.

Ce qu'il affiche, pour une ou plusieurs cartes (liste « Carte » en haut) :
- dernières valeurs : température, humidité, pression, alimentation, heure du dernier relevé ;
- courbes : température, humidité, pression, qualité d'air (CO₂ équivalent, COV), LiPo de la carte (V3) ;
- bandeau secteur / batterie dans le temps ;
- tableau des 200 derniers relevés.

Les relevés sont placés à leur heure de mesure sur la carte (`mesure_le`), ou à leur heure d'arrivée si la carte n'était pas à l'heure : les relevés publiés en retard après une coupure Wi-Fi se rangent donc au bon endroit.

## 1. Créer le compte de lecture dans Supabase

1. Supabase > **SQL Editor** > New query.
2. Coller `supabase/grafana_lecture.sql`.
3. Remplacer `CHANGER-CE-MOT-DE-PASSE` par un mot de passe long (le noter), puis **Run**.

Ce compte (`grafana_lecture`) ne peut que lire la table des relevés : toute écriture est refusée, et une requête est coupée au bout de 30 s.

## 2. Relever les paramètres de connexion

Supabase > bouton **Connect** (en haut) > **Session pooler**. Noter :
- l'**hôte** : `aws-X-<région>.pooler.supabase.com` ;
- la **référence du projet** : la suite de lettres après `postgres.` dans le nom d'utilisateur (aussi dans Project Settings > General > Project ID).

Le « Session pooler » est obligatoire : la connexion directe (`db.<réf>.supabase.co`) n'est joignable qu'en IPv6 sur l'offre gratuite.

## 3. Créer un compte Grafana Cloud

Sur grafana.com : **Create free account** (offre gratuite, sans carte bancaire), puis créer une « stack » (le nom choisi donne l'adresse `https://<nom>.grafana.net`).

## 4. Ajouter la source de données

Grafana > **Connections > Data sources > Add new data source > PostgreSQL** :

| Champ | Valeur |
|---|---|
| Name | `Supabase caravane` |
| Host URL | `aws-X-<région>.pooler.supabase.com:5432` |
| Database name | `postgres` |
| Username | `grafana_lecture.<référence-du-projet>` (le suffixe est obligatoire) |
| Password | le mot de passe de l'étape 1 |
| TLS/SSL Mode | `require` |
| Version | la plus haute proposée (15 ou plus) |
| TimescaleDB | désactivé |

**Save & test** doit afficher « Database Connection OK ».
Erreur « Tenant or user not found » : il manque `.<référence-du-projet>` après `grafana_lecture`.

## 5. Importer le tableau de bord

Grafana > **Dashboards > New > Import** > **Upload dashboard JSON file** > `grafana/caravane-releves.json` > choisir la source « Supabase caravane » > **Import**.

Le tableau se rafraîchit toutes les 5 minutes et montre les 2 derniers jours par défaut (sélecteur de période en haut à droite).

## Sans Grafana

Pour un coup d'œil rapide : Supabase > **Table Editor** > `caravane_mesures` (tri par `recu_le`), ou la vue `caravane_cartes` (dernier relevé et nom de chaque carte).
