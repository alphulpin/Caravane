-- Agrégats des séries temporelles et rétention.
--
-- À exécuter APRÈS caravane_releves.sql (SQL Editor > New query > coller > Run). Relançable.
--
--   caravane_agregats   : par carte, grandeur et tranche (heure ou jour, en heure de Paris) :
--                         nombre de points, moyenne, minimum, maximum
--   caravane_agreger()  : (re)calcule les tranches touchées par les relevés arrivés depuis une date
--                         (un relevé gardé en mémoire et publié avec retard est donc bien pris en compte)
--   caravane_purger()   : supprime les relevés bruts au-delà de la durée de conservation ;
--                         les agrégats, eux, sont gardés indéfiniment
--
-- Tâches planifiées (extension pg_cron, fournie par Supabase) :
--   toutes les 15 min : agrégats des tranches touchées pendant les dernières 24 h
--   le 1er du mois à 3 h 17 (UTC) : purge des relevés bruts de plus d'un an
--
-- Pourquoi pas TimescaleDB : l'extension n'existe plus sur Supabase à partir de Postgres 17.

create table if not exists public.caravane_agregats (
  periode    text        not null check (periode in ('heure', 'jour')),
  debut      timestamptz not null,           -- début de la tranche (jour = minuit heure de Paris)
  appareil   text        not null,
  grandeur   text        not null,
  nb         integer     not null,           -- nombre de relevés dans la tranche
  moyenne    double precision,
  mini       double precision,
  maxi       double precision,
  calcule_le timestamptz not null default now(),
  primary key (periode, appareil, grandeur, debut)
);
create index if not exists caravane_agregats_debut on public.caravane_agregats (periode, debut);

-- (Re)calcule les tranches horaires et journalières qui contiennent au moins un relevé reçu
-- depuis « p_depuis ». Renvoie le nombre de lignes d'agrégats écrites.
create or replace function public.caravane_agreger(p_depuis timestamptz default '-infinity')
returns integer
language plpgsql
set search_path = public
as $$
declare
  n_heure integer;
  n_jour  integer;
begin
  -- Tranches horaires
  with touchees as (
    select distinct appareil, date_trunc('hour', horodatage) as debut
      from caravane_releves
     where recu_le >= p_depuis
  ), valeurs as (
    select t.debut, r.appareil, v.key as grandeur, (v.value #>> '{}')::double precision as valeur
      from touchees t
      join caravane_releves r
        on r.appareil = t.appareil and r.horodatage >= t.debut and r.horodatage < t.debut + interval '1 hour'
     cross join lateral jsonb_each(r.valeurs) v
     where jsonb_typeof(v.value) = 'number'
  )
  insert into caravane_agregats as a (periode, debut, appareil, grandeur, nb, moyenne, mini, maxi)
  select 'heure', debut, appareil, grandeur, count(*), avg(valeur), min(valeur), max(valeur)
    from valeurs
   group by debut, appareil, grandeur
  on conflict (periode, appareil, grandeur, debut) do update
     set nb = excluded.nb, moyenne = excluded.moyenne, mini = excluded.mini, maxi = excluded.maxi,
         calcule_le = now();
  get diagnostics n_heure = row_count;

  -- Tranches journalières (journée civile à Paris), à partir des relevés bruts
  with touchees as (
    select distinct appareil, date_trunc('day', horodatage, 'Europe/Paris') as debut
      from caravane_releves
     where recu_le >= p_depuis
  ), valeurs as (
    select t.debut, r.appareil, v.key as grandeur, (v.value #>> '{}')::double precision as valeur
      from touchees t
      join caravane_releves r
        on r.appareil = t.appareil and r.horodatage >= t.debut
       and r.horodatage < (t.debut at time zone 'Europe/Paris' + interval '1 day') at time zone 'Europe/Paris'
     cross join lateral jsonb_each(r.valeurs) v
     where jsonb_typeof(v.value) = 'number'
  )
  insert into caravane_agregats as a (periode, debut, appareil, grandeur, nb, moyenne, mini, maxi)
  select 'jour', debut, appareil, grandeur, count(*), avg(valeur), min(valeur), max(valeur)
    from valeurs
   group by debut, appareil, grandeur
  on conflict (periode, appareil, grandeur, debut) do update
     set nb = excluded.nb, moyenne = excluded.moyenne, mini = excluded.mini, maxi = excluded.maxi,
         calcule_le = now();
  get diagnostics n_jour = row_count;

  return n_heure + n_jour;
end $$;

-- Supprime les relevés bruts plus vieux que « p_conserver » (agrégats conservés).
-- Les tranches concernées doivent avoir été agrégées : c'est le cas, l'agrégation tournant
-- toutes les 15 min. Renvoie le nombre de relevés supprimés.
create or replace function public.caravane_purger(p_conserver interval default interval '1 year')
returns integer
language plpgsql
set search_path = public
as $$
declare
  n integer;
begin
  delete from caravane_releves where horodatage < now() - p_conserver;
  get diagnostics n = row_count;
  return n;
end $$;

-- Ces fonctions ne sont pas pour la clé publique des cartes
revoke all on function public.caravane_agreger(timestamptz) from public, anon, authenticated;
revoke all on function public.caravane_purger(interval)    from public, anon, authenticated;

-- Sécurité de la table : lecture seulement (tableau de bord et Grafana)
alter table public.caravane_agregats enable row level security;
revoke all on public.caravane_agregats from anon;
drop policy if exists "lecture authentifiée" on public.caravane_agregats;
create policy "lecture authentifiée" on public.caravane_agregats for select to authenticated using (true);
grant select on public.caravane_agregats to authenticated;

drop view if exists public.caravane_series_agregees;   -- (version précédente)

do $$
begin
  if exists (select 1 from pg_roles where rolname = 'grafana_lecture') then
    execute 'grant select on public.caravane_agregats to grafana_lecture';
    execute 'drop policy if exists "grafana : lecture" on public.caravane_agregats';
    execute 'create policy "grafana : lecture" on public.caravane_agregats for select to grafana_lecture using (true)';
  end if;
end $$;

-- Calcul initial de tout l'historique
select public.caravane_agreger('-infinity') as "agrégats calculés";

-- Tâches planifiées (pg_cron). Sur Supabase, l'extension s'active ici ou dans
-- Dashboard > Integrations > Cron. Ignoré si pg_cron n'est pas disponible.
do $$
begin
  if not exists (select 1 from pg_available_extensions where name = 'pg_cron') then
    raise notice 'pg_cron indisponible : tâches planifiées non créées';
    return;
  end if;
  create extension if not exists pg_cron with schema pg_catalog;
  -- cron.schedule remplace une tâche existante du même nom
  perform cron.schedule('caravane-agregats', '*/15 * * * *',
                        $c$select public.caravane_agreger(now() - interval '1 day')$c$);
  perform cron.schedule('caravane-purge', '17 3 1 * *',
                        $c$select public.caravane_purger(interval '1 year')$c$);
  raise notice 'Tâches planifiées caravane-agregats et caravane-purge en place';
end $$;

-- Suivi des tâches :
--   select jobname, schedule, active from cron.job where jobname like 'caravane-%';
--   select jobid, status, return_message, start_time from cron.job_run_details order by start_time desc limit 10;
-- Autre durée de conservation (ex. 2 ans) :
--   select cron.schedule('caravane-purge', '17 3 1 * *', $c$select public.caravane_purger(interval '2 years')$c$);
