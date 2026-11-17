-- Table des mesures de la carte caravane, alimentée :
--   - directement par la carte en mode Wi-Fi (POST /rest/v1/caravane_mesures, clé publique),
--   - plus tard par le webhook TTN (mode LoRa).
-- À exécuter une fois dans Supabase : SQL Editor > New query > coller > Run.
-- La base est partagée avec le projet Apnée : tout est préfixé « caravane_ ».

create table if not exists public.caravane_mesures (
  id          bigint generated always as identity primary key,
  recu_le     timestamptz not null default now(),   -- heure d'arrivée dans Supabase
  mesure_le   timestamptz,                          -- heure de la carte (absente si non réglée)
  appareil    text not null,                        -- "caravane-a1b2c3" (adresse MAC)
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

create index if not exists caravane_mesures_appareil_date
  on public.caravane_mesures (appareil, coalesce(mesure_le, recu_le) desc);

-- Sécurité : la clé publique embarquée dans la carte ne peut QU'insérer.
-- Aucune lecture, modification ni suppression avec cette clé.
alter table public.caravane_mesures enable row level security;

drop policy if exists "carte : insertion seule" on public.caravane_mesures;
create policy "carte : insertion seule"
  on public.caravane_mesures
  for insert
  to anon
  with check (
    appareil like 'caravane-%'
    and source in ('wifi', 'lora')
  );

-- Droits explicites (Supabase les accorde déjà par défaut à anon ; on les restreint à l'insertion)
revoke all on public.caravane_mesures from anon;
grant insert on public.caravane_mesures to anon;

-- Lecture : depuis le tableau de bord Supabase (rôle service) ou un utilisateur authentifié.
drop policy if exists "lecture authentifiée" on public.caravane_mesures;
create policy "lecture authentifiée"
  on public.caravane_mesures
  for select
  to authenticated
  using (true);
grant select on public.caravane_mesures to authenticated;
