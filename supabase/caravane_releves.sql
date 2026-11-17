-- Relevés des cartes caravane, en séries temporelles souples.
--
--   caravane_releves   : une ligne par relevé (en-tête + valeurs mesurées en JSON)
--   caravane_grandeurs : catalogue des grandeurs (libellé, unité), pour l'affichage
--   caravane_series    : vue « appareil / grandeur / date / valeur » pour Grafana
--   caravane_cartes    : vue du dernier nom connu de chaque carte
--
-- Alimentée directement par les cartes en Wi-Fi (POST /rest/v1/caravane_releves, clé publique)
-- et, plus tard, par le webhook TTN (LoRa), qui écrira exactement le même format.
-- Un nouveau capteur = une clé de plus dans « valeurs » (et une ligne dans caravane_grandeurs
-- pour un joli libellé) : aucune colonne à ajouter.
--
-- À exécuter dans Supabase : SQL Editor > New query > coller > Run. Relançable sans risque.
-- Reprend automatiquement les lignes de l'ancienne table caravane_mesures si elle existe.
-- La base est partagée avec le projet Apnée : tout est préfixé « caravane_ ».

create table if not exists public.caravane_releves (
  id          bigint generated always as identity primary key,
  recu_le     timestamptz not null default now(),   -- arrivée dans Supabase
  mesure_le   timestamptz,                          -- heure du relevé sur la carte (null si inconnue)
  appareil    text not null,                        -- n° de série de la carte (MAC, 12 hexa)
  releve_id   bigint not null,                      -- n° de relevé propre à la carte
  nom         text,                                 -- nom donné à la carte
  carte       text,                                 -- "V2" ou "V3"
  dev_eui     text,                                 -- identifiant LoRaWAN (V3)
  source      text not null default 'wifi',         -- "wifi" ou "lora"
  secteur     boolean,                              -- caravane branchée sur le 230 V
  valeurs     jsonb not null default '{}'::jsonb    -- {"temp_c":21.4,"hum_pct":55,...}
);

-- Un relevé n'est jamais enregistré deux fois : un doublon est refusé (HTTP 409) et la carte
-- considère alors ce relevé comme publié. (Pas de « on conflict » : il exigerait le droit de
-- lecture pour la clé publique.)
create unique index if not exists caravane_releves_unique on public.caravane_releves (appareil, releve_id);
create index if not exists caravane_releves_appareil_date
  on public.caravane_releves (appareil, coalesce(mesure_le, recu_le) desc);

create table if not exists public.caravane_grandeurs (
  code     text primary key,       -- clé utilisée dans « valeurs »
  libelle  text not null,
  unite    text,
  ordre    integer not null default 100
);

insert into public.caravane_grandeurs (code, libelle, unite, ordre) values
  ('temp_c',    'Température',          '°C',  10),
  ('hum_pct',   'Humidité',             '%',   20),
  ('press_hpa', 'Pression',             'hPa', 30),
  ('eco2_ppm',  'CO2 équivalent',       'ppm', 40),
  ('tvoc_ppb',  'COV',                  'ppb', 50),
  ('vbat_mv',   'LiPo de la carte',     'mV',  60),
  ('bat12_mv',  'Batterie caravane',    'mV',  70),   -- à venir
  ('bat12_ma',  'Courant batterie',     'mA',  80)    -- à venir
on conflict (code) do update set libelle = excluded.libelle, unite = excluded.unite, ordre = excluded.ordre;

-- Format long pour Grafana : une ligne par valeur numérique. Une clé inconnue du catalogue
-- apparaît quand même (libellé = code).
create or replace view public.caravane_series
  with (security_invoker = true) as
  select coalesce(r.mesure_le, r.recu_le)          as mesure_le,
         r.appareil,
         coalesce(r.nom, r.appareil)               as nom,
         v.key                                     as grandeur,
         coalesce(g.libelle, v.key)                as libelle,
         g.unite,
         (v.value #>> '{}')::double precision      as valeur,
         r.secteur,
         r.source,
         r.releve_id
    from public.caravane_releves r
   cross join lateral jsonb_each(r.valeurs) v
    left join public.caravane_grandeurs g on g.code = v.key
   where jsonb_typeof(v.value) = 'number';

create or replace view public.caravane_cartes
  with (security_invoker = true) as
  select distinct on (appareil)
         appareil, nom, carte, dev_eui, recu_le as dernier_releve
    from public.caravane_releves
   order by appareil, recu_le desc;

-- Reprise de l'ancienne table « une colonne par capteur » (si elle existe).
do $$
begin
  if to_regclass('public.caravane_mesures') is not null then
    insert into public.caravane_releves
           (recu_le, mesure_le, appareil, releve_id, nom, carte, dev_eui, source, secteur, valeurs)
    select recu_le, mesure_le, appareil, releve_id, nom, carte, dev_eui, source, secteur,
           jsonb_strip_nulls(jsonb_build_object(
             'temp_c', temp_c, 'hum_pct', hum_pct, 'press_hpa', press_hpa,
             'eco2_ppm', eco2_ppm, 'tvoc_ppb', tvoc_ppb, 'vbat_mv', vbat_mv))
      from public.caravane_mesures
     where releve_id is not null and appareil ~ '^[0-9A-F]{12}$'
    on conflict (appareil, releve_id) do nothing;
  end if;
end $$;

-- Sécurité : la clé publique embarquée dans les cartes ne peut QU'insérer des relevés.
alter table public.caravane_releves   enable row level security;
alter table public.caravane_grandeurs enable row level security;

drop policy if exists "carte : insertion seule" on public.caravane_releves;
create policy "carte : insertion seule"
  on public.caravane_releves
  for insert
  to anon
  with check (
    appareil ~ '^[0-9A-F]{12}$'
    and source in ('wifi', 'lora')
    and jsonb_typeof(valeurs) = 'object'
    and octet_length(valeurs::text) <= 2000
  );

revoke all on public.caravane_releves   from anon;
revoke all on public.caravane_grandeurs from anon;
revoke all on public.caravane_series    from anon;
revoke all on public.caravane_cartes    from anon;
grant insert on public.caravane_releves to anon;

-- Lecture : tableau de bord Supabase (rôle service) ou utilisateur authentifié.
drop policy if exists "lecture authentifiée" on public.caravane_releves;
create policy "lecture authentifiée" on public.caravane_releves for select to authenticated using (true);
drop policy if exists "lecture authentifiée" on public.caravane_grandeurs;
create policy "lecture authentifiée" on public.caravane_grandeurs for select to authenticated using (true);
grant select on public.caravane_releves, public.caravane_grandeurs,
                public.caravane_series, public.caravane_cartes to authenticated;

-- Lecture pour Grafana (si le rôle a été créé avec grafana_lecture.sql)
do $$
begin
  if exists (select 1 from pg_roles where rolname = 'grafana_lecture') then
    execute 'grant select on public.caravane_releves, public.caravane_grandeurs,
                             public.caravane_series, public.caravane_cartes to grafana_lecture';
    execute 'drop policy if exists "grafana : lecture" on public.caravane_releves';
    execute 'create policy "grafana : lecture" on public.caravane_releves for select to grafana_lecture using (true)';
    execute 'drop policy if exists "grafana : lecture" on public.caravane_grandeurs';
    execute 'create policy "grafana : lecture" on public.caravane_grandeurs for select to grafana_lecture using (true)';
  end if;
end $$;

-- Une fois les cartes passées au nouveau firmware et les données vérifiées, l'ancienne table
-- peut être supprimée (la vue caravane_cartes pointe déjà sur la nouvelle) :
--   drop table public.caravane_mesures;
