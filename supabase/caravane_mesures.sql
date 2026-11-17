-- Table des relevés des cartes caravane, alimentée :
--   - directement par les cartes en mode Wi-Fi (POST /rest/v1/caravane_mesures, clé publique),
--   - plus tard par le webhook TTN (mode LoRa).
-- À exécuter dans Supabase : SQL Editor > New query > coller > Run.
-- Le script peut être relancé sans risque : il crée la table ou met à jour une version précédente.
-- La base est partagée avec le projet Apnée : tout est préfixé « caravane_ ».

create table if not exists public.caravane_mesures (
  id          bigint generated always as identity primary key,
  recu_le     timestamptz not null default now(),   -- heure d'arrivée dans Supabase
  mesure_le   timestamptz,                          -- heure du relevé sur la carte (absente si non réglée)
  appareil    text not null,                        -- n° de série de la carte (adresse MAC, 12 hexa)
  carte       text,                                 -- "V2" ou "V3"
  source      text not null default 'wifi',         -- "wifi" ou "lora"
  secteur     boolean,
  vbat_mv     integer,                              -- LiPo de la carte (V3)
  temp_c      real,
  hum_pct     real,
  press_hpa   real,
  eco2_ppm    integer,                              -- seulement capteur chaud (secteur ou Wi-Fi)
  tvoc_ppb    integer
);

-- Colonnes ajoutées avec le nom de carte et le journal des relevés
alter table public.caravane_mesures add column if not exists nom       text;     -- nom donné à la carte
alter table public.caravane_mesures add column if not exists dev_eui   text;     -- identifiant LoRaWAN (V3)
alter table public.caravane_mesures add column if not exists releve_id bigint;   -- n° de relevé propre à la carte

-- Un relevé n'est jamais enregistré deux fois (renvoi après une réponse perdue, Wi-Fi puis LoRa…).
-- Un doublon est refusé (HTTP 409) : la carte renvoie alors son lot ligne par ligne et
-- considère comme publiées celles qui étaient déjà là. (Pas de « on conflict » : il obligerait
-- à donner le droit de lecture à la clé publique.)
create unique index if not exists caravane_mesures_releve_unique
  on public.caravane_mesures (appareil, releve_id);

create index if not exists caravane_mesures_appareil_date
  on public.caravane_mesures (appareil, coalesce(mesure_le, recu_le) desc);

-- Dernier nom connu de chaque carte (pratique pour Grafana ou un tableau de bord)
create or replace view public.caravane_cartes
  with (security_invoker = true) as
  select distinct on (appareil)
         appareil, nom, carte, dev_eui, recu_le as dernier_releve
    from public.caravane_mesures
   order by appareil, recu_le desc;

-- Sécurité : la clé publique embarquée dans les cartes ne peut QU'insérer.
-- Aucune lecture, modification ni suppression avec cette clé.
alter table public.caravane_mesures enable row level security;

drop policy if exists "carte : insertion seule" on public.caravane_mesures;
create policy "carte : insertion seule"
  on public.caravane_mesures
  for insert
  to anon
  with check (
    appareil ~ '^[0-9A-F]{12}$'
    and source in ('wifi', 'lora')
  );

revoke all on public.caravane_mesures from anon;
grant insert on public.caravane_mesures to anon;
revoke all on public.caravane_cartes from anon;

-- Lecture : depuis le tableau de bord Supabase (rôle service) ou un utilisateur authentifié.
drop policy if exists "lecture authentifiée" on public.caravane_mesures;
create policy "lecture authentifiée"
  on public.caravane_mesures
  for select
  to authenticated
  using (true);
grant select on public.caravane_mesures to authenticated;
grant select on public.caravane_cartes to authenticated;
