-- Réception des relevés LoRa : le webhook TTN appelle la fonction caravane_ttn, qui range
-- chaque relevé décodé dans caravane_releves (source « lora »), comme le Wi-Fi.
--
-- À exécuter APRÈS caravane_releves.sql (SQL Editor > New query > coller > Run). Relançable.
--
-- Webhook TTN (Application > Integrations > Webhooks > Custom webhook) :
--   Base URL          https://<projet>.supabase.co/rest/v1/rpc
--   Uplink message    /caravane_ttn          (laisser les autres types de message décochés)
--   En-têtes          apikey: <clé publishable>   et   Authorization: Bearer <clé publishable>
--   Format            JSON
-- Voir docs/passerelle-ttn.md.
--
-- TTN envoie le message complet (identifiants, métadonnées radio, charge décodée par
-- ttn/decodeur.js). On en garde :
--   appareil   = n° de série retrouvé depuis le DevEUI (on retire le « FFFE » central)
--   releve_id  = n° de relevé de la carte (un relevé reçu deux fois n'est enregistré qu'une fois)
--   mesure_le  = heure de réception TTN (la trame ne transporte pas l'heure)
--   valeurs    = valeurs décodées + qualité radio (lora_rssi, lora_snr : meilleure passerelle)
-- Port 2 : la trame porte en plus la configuration de la carte (démarrage, changement,
-- battement quotidien), rangée dans caravane_config comme celle publiée en Wi-Fi.

-- Paramètre unique sans nom, de type jsonb : PostgREST (l'API de Supabase) lui passe alors le
-- corps entier de la requête, tel que TTN l'envoie.
create or replace function public.caravane_ttn(jsonb)
returns void
language plpgsql
security definer                       -- la clé publique n'a pas le droit de lire la table
set search_path = public
as $$
declare
  message  jsonb := $1;
  dev_eui  text := upper(message #>> '{end_device_ids,dev_eui}');
  montant  jsonb := message -> 'uplink_message';
  charge   jsonb := montant -> 'decoded_payload';
  radio    jsonb;
  valeurs  jsonb;
  v_appareil text;
  port     int;
begin
  -- Seuls les relevés (port 1) correctement décodés nous intéressent
  if dev_eui is null or dev_eui !~ '^[0-9A-F]{6}FFFE[0-9A-F]{6}$' then
    raise exception 'DevEUI absent ou inattendu : %', dev_eui using errcode = '22023';
  end if;
  port := coalesce((montant ->> 'f_port')::int, 0);
  if port not in (1, 2) or charge is null
     or jsonb_typeof(charge -> 'valeurs') <> 'object' or (charge ->> 'releve_id') is null then
    return;                            -- autre port, ou trame non décodée : ignorée sans erreur
  end if;

  -- Qualité radio de la meilleure passerelle
  select r into radio
    from jsonb_array_elements(coalesce(montant -> 'rx_metadata', '[]'::jsonb)) r
   order by (r ->> 'rssi')::numeric desc nulls last
   limit 1;
  valeurs := charge -> 'valeurs';
  if radio ? 'rssi' then valeurs := valeurs || jsonb_build_object('lora_rssi', (radio ->> 'rssi')::numeric); end if;
  if radio ? 'snr'  then valeurs := valeurs || jsonb_build_object('lora_snr',  (radio ->> 'snr')::numeric);  end if;

  v_appareil := substr(dev_eui, 1, 6) || substr(dev_eui, 11, 6);
  insert into caravane_releves (appareil, releve_id, source, mesure_le, secteur, dev_eui, valeurs)
  values (v_appareil,
          (charge ->> 'releve_id')::bigint,
          'lora',
          coalesce((message ->> 'received_at')::timestamptz, now()),
          (charge ->> 'secteur')::boolean,
          dev_eui,
          valeurs)
  on conflict (appareil, releve_id) do nothing;

  -- Configuration jointe (port 2)
  if port = 2 and jsonb_typeof(charge -> 'config') = 'object' then
    insert into caravane_config (appareil, recu_le, source, evenement, config)
    values (v_appareil,
            coalesce((message ->> 'received_at')::timestamptz, now()),
            'lora',
            coalesce(charge ->> 'evenement', 'periodique'),
            (charge -> 'config')
              || jsonb_build_object('carte', 'V3', 'dev_eui', dev_eui,
                                    'ttn_id', message #>> '{end_device_ids,device_id}')
              || jsonb_build_object('etat', coalesce(charge -> 'etat', '{}'::jsonb)
                                            || jsonb_strip_nulls(jsonb_build_object('lora_rssi', radio -> 'rssi',
                                                                                    'lora_snr',  radio -> 'snr'))));
  end if;
end $$;

-- Appelable avec la clé publique (c'est elle que porte le webhook TTN), et seulement ainsi
revoke all on function public.caravane_ttn(jsonb) from public;
grant execute on function public.caravane_ttn(jsonb) to anon, authenticated;

-- Grandeurs de qualité radio, pour l'affichage
insert into public.caravane_grandeurs (code, libelle, unite, ordre) values
  ('lora_rssi', 'Signal LoRa (RSSI)', 'dBm', 90),
  ('lora_snr',  'Signal LoRa (SNR)',  'dB',  91)
on conflict (code) do update set libelle = excluded.libelle, unite = excluded.unite, ordre = excluded.ordre;
