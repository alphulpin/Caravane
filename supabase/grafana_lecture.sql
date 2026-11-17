-- Compte de lecture seule pour Grafana (ou tout autre outil de visualisation).
-- À exécuter dans Supabase : SQL Editor > New query > coller > remplacer le mot de passe > Run.
-- Peut être relancé : il met le mot de passe à jour.
--
-- Connexion depuis Grafana (session pooler, compatible IPv4) :
--   hôte   : aws-X-<région>.pooler.supabase.com   (bouton Connect > Session pooler)
--   port   : 5432
--   base   : postgres
--   compte : grafana_lecture.<référence-du-projet>   (le suffixe est obligatoire sur le pooler)
--   TLS    : require

do $$
begin
  if not exists (select 1 from pg_roles where rolname = 'grafana_lecture') then
    create role grafana_lecture login;
  end if;
end $$;

-- ⚠ Remplacer par un mot de passe long et unique (et le noter : il sera demandé dans Grafana).
alter role grafana_lecture with login password 'CHANGER-CE-MOT-DE-PASSE';

-- Lecture seule, requêtes limitées à 30 s (une erreur de tableau de bord ne peut rien casser).
alter role grafana_lecture set default_transaction_read_only = on;
alter role grafana_lecture set statement_timeout = '30s';

grant usage on schema public to grafana_lecture;
grant select on public.caravane_mesures to grafana_lecture;
grant select on public.caravane_cartes  to grafana_lecture;

-- La table est protégée par RLS : il faut une politique de lecture pour ce compte.
drop policy if exists "grafana : lecture" on public.caravane_mesures;
create policy "grafana : lecture"
  on public.caravane_mesures
  for select
  to grafana_lecture
  using (true);
