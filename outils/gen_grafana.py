# Génère grafana/caravane-releves.json (tableau de bord importable) à partir des vues
# caravane_series / caravane_releves / caravane_cartes. Lancer : python3 outils/gen_grafana.py
import json, os
ICI = os.path.dirname(os.path.abspath(__file__))
DS = {"type": "grafana-postgresql-datasource", "uid": "${DS_SUPABASE}"}

def cible(sql, fmt="time_series", ref="A"):
    return {"datasource": DS, "refId": ref, "editorMode": "code", "format": fmt, "rawQuery": True, "rawSql": sql}

pid = [0]
def panneau(typ, titre, x, y, w, h, cibles, defauts=None, opts=None, **autres):
    pid[0] += 1
    p = {"id": pid[0], "type": typ, "title": titre, "datasource": DS,
         "gridPos": {"x": x, "y": y, "w": w, "h": h}, "targets": cibles,
         "fieldConfig": {"defaults": defauts or {}, "overrides": []}, "options": opts or {}}
    # Unité appliquée à une seule colonne : en défaut, elle s'appliquerait aussi au nom de la carte
    # (colonne texte), que Grafana afficherait alors « NaN »
    unite = autres.pop("unite_champ", None)
    if unite:
        p["fieldConfig"]["overrides"].append({
            "matcher": {"id": "byName", "options": unite[0]},
            "properties": [{"id": "unit", "value": unite[1]}]})
    if autres.pop("overrides_minmax", False):
        # minimum / maximum des tranches agrégées : trait fin pointillé, sans remplissage
        p["fieldConfig"]["overrides"].append({
            "matcher": {"id": "byRegexp", "options": ".*· (min|max)$"},
            "properties": [{"id": "custom.lineStyle", "value": {"fill": "dash", "dash": [6, 4]}},
                           {"id": "custom.lineWidth", "value": 1},
                           {"id": "custom.fillOpacity", "value": 0}]})
    p.update(autres)
    return p

ts_def = {"custom": {"lineWidth": 2, "fillOpacity": 8, "showPoints": "auto", "spanNulls": False,
                     "drawStyle": "line", "lineInterpolation": "smooth"}}
ts_opts = {"legend": {"displayMode": "list", "placement": "bottom", "showLegend": True},
           "tooltip": {"mode": "multi", "sort": "none"}}
alim = {"mappings": [{"type": "value", "options": {"0": {"text": "Batterie", "color": "orange"},
                                                   "1": {"text": "Secteur", "color": "green"}}}]}
T = "r.horodatage"
CARTE = "coalesce(k.nom, r.appareil)"
JOIN = "caravane_releves r left join caravane_cartes k using (appareil)"

panels = [
  # Dernière valeur de chaque grandeur choisie, pour chaque carte choisie
  panneau("stat", "Dernières valeurs", 0, 0, 18, 5,
          [cible("select distinct on (appareil, grandeur)\n"
                 "  nom || ' · ' || libelle || coalesce(' (' || unite || ')', '') as \"grandeur\", valeur\n"
                 "from caravane_series\n"
                 "where appareil in ($carte) and grandeur in ($grandeur)\n"
                 "order by appareil, grandeur, horodatage desc", "table")],
          {"decimals": 1, "color": {"mode": "fixed", "fixedColor": "blue"}},
          {"reduceOptions": {"calcs": ["lastNotNull"], "fields": "/^valeur$/", "values": True},
           "colorMode": "value", "graphMode": "none", "textMode": "value_and_name", "justifyMode": "auto",
           "orientation": "auto", "showPercentChange": False}),
  panneau("stat", "Dernier relevé", 18, 0, 6, 5,
          [cible("select " + CARTE + " as carte, extract(epoch from max(" + T + ")) * 1000 as \"dernier relevé\"\n"
                 "from " + JOIN + "\nwhere r.appareil in ($carte)\ngroup by 1", "table")],
          {},
          {"reduceOptions": {"calcs": ["lastNotNull"], "fields": "/^dernier relevé$/", "values": True},
           "colorMode": "none", "graphMode": "none", "textMode": "value_and_name"},
          unite_champ=("dernier relevé", "dateTimeFromNow")),
  # Une courbe par grandeur choisie (panneau répété), une série par carte
  # Résolution automatique selon la plage affichée : relevés bruts jusqu'à 7 jours, moyennes
  # horaires jusqu'à 90 jours, journalières au-delà (avec minimum et maximum en pointillés).
  panneau("timeseries", "${grandeur:text}", 0, 5, 12, 8,
          [cible("with plage as (select $__timeTo()::timestamptz - $__timeFrom()::timestamptz as d),\n"
                 "resolution as (\n"
                 "  select case when d <= interval '7 days' then 'brut'\n"
                 "              when d <= interval '90 days' then 'heure' else 'jour' end as r,\n"
                 "         case when d <= interval '7 days' then interval '1 hour'\n"
                 "              when d <= interval '90 days' then interval '3 hours' else interval '2 days' end as trou\n"
                 "  from plage),\n"
                 "points as (\n"
                 "  select s.horodatage as time, s.nom as metric, s.valeur as value\n"
                 "  from caravane_series s, resolution\n"
                 "  where resolution.r = 'brut'\n"
                 "    and s.appareil in ($carte) and s.grandeur = '$grandeur' and $__timeFilter(s.horodatage)\n"
                 "  union all\n"
                 "  select a.debut, coalesce(k.nom, a.appareil) || m.suffixe,\n"
                 "         case m.suffixe when '' then a.moyenne when ' · min' then a.mini else a.maxi end\n"
                 "  from caravane_agregats a\n"
                 "  left join caravane_cartes k using (appareil)\n"
                 "  cross join resolution\n"
                 "  cross join (values (''), (' · min'), (' · max')) m(suffixe)\n"
                 "  where a.periode = resolution.r\n"
                 "    and a.appareil in ($carte) and a.grandeur = '$grandeur' and $__timeFilter(a.debut)),\n"
                 "trous as (\n"
                 "  select p.time + interval '1 second' as time, p.metric, null::double precision as value\n"
                 "  from (select time, metric, lead(time) over (partition by metric order by time) as suivant from points) p,\n"
                 "       resolution\n"
                 "  where p.suivant - p.time > resolution.trou)\n"
                 "select * from points\nunion all\nselect * from trous\norder by 1")],
          ts_def, ts_opts, repeat="grandeur", repeatDirection="h", maxPerRow=2,
          overrides_minmax=True,
          description="Jusqu'à 7 jours : chaque relevé. Jusqu'à 90 jours : moyenne horaire. Au-delà : moyenne journalière. "
                      "En vue agrégée, minimum et maximum de la tranche en pointillés."),
  panneau("state-timeline", "Alimentation", 0, 13, 24, 4,
          [cible("select " + T + " as time, " + CARTE + " as metric,\n"
                 "  case when r.secteur then 1 else 0 end as value\n"
                 "from " + JOIN + "\n"
                 "where r.appareil in ($carte) and r.secteur is not null and $__timeFilter(" + T + ")\norder by 1")],
          {**alim, "custom": {"fillOpacity": 80, "lineWidth": 0}},
          {"mergeValues": True, "showValue": "never", "alignValue": "left", "rowHeight": 0.8,
           "legend": {"showLegend": False}}),
  panneau("table", "Derniers relevés", 0, 17, 24, 9,
          [cible("select " + T + " as \"heure\", " + CARTE + " as \"carte\", r.releve_id as \"n°\", r.source,\n"
                 "  case when r.secteur then 'secteur' else 'batterie' end as \"alim.\",\n"
                 "  r.valeurs::text as \"valeurs\", r.recu_le as \"reçu le\"\n"
                 "from " + JOIN + "\nwhere r.appareil in ($carte) and $__timeFilter(" + T + ")\n"
                 "order by 1 desc\nlimit 200", "table")],
          None, {"showHeader": True, "cellHeight": "sm", "footer": {"show": False}}),
  panneau("table", "Configuration publiée (démarrages, changements, battement quotidien)", 0, 26, 24, 8,
          [cible("select k.recu_le as \"reçu le\", coalesce(nullif(k.config->>'nom', ''), k.appareil) as \"carte\",\n"
                 "  k.evenement as \"événement\", k.config->>'cause' as \"cause\", k.config->>'firmware' as \"firmware\",\n"
                 "  k.config->>'comm' as \"comm.\", (k.config->>'releve_s')::int / 60 as \"relevé (min)\",\n"
                 "  round((k.config->'etat'->>'vbat_mv')::numeric / 1000, 2) as \"LiPo (V)\",\n"
                 "  case (k.config->'etat'->>'secteur')::boolean when true then 'secteur' when false then 'batterie' end as \"alim.\",\n"
                 "  (k.config->'etat'->>'wifi_rssi')::int as \"Wi-Fi (dBm)\",\n"
                 "  (select string_agg(e.key || ' ×' || (e.value->>'pente') || ' ' || (e.value->>'decalage'), ', ' order by e.key)\n"
                 "     from jsonb_each(k.config->'etalonnage') e\n"
                 "    where (e.value->>'pente')::float <> 1 or (e.value->>'decalage')::float <> 0) as \"étalonnage (≠ défaut)\"\n"
                 "from caravane_config k\nwhere k.appareil in ($carte) and $__timeFilter(k.recu_le)\n"
                 "order by k.recu_le desc\nlimit 100", "table")],
          None, {"showHeader": True, "cellHeight": "sm", "footer": {"show": False}}),
]

ANNOT = ("select recu_le as time, coalesce(nullif(config->>'nom', ''), appareil) || ' : ' || evenement as text\n"
         "from caravane_config\nwhere appareil in ($carte) and evenement <> 'periodique' and $__timeFilter(recu_le)")
var_carte = "select coalesce(nom, appareil) as __text, appareil as __value from caravane_cartes order by 1"
var_grandeur = ("select libelle || coalesce(' (' || unite || ')', '') as __text, code as __value\n"
                "from caravane_grandeurs g\n"
                "where exists (select 1 from caravane_releves r where r.valeurs ? g.code)\n"
                "order by ordre")
def variable(nom, libelle, sql):
    return {"name": nom, "label": libelle, "type": "query", "datasource": DS, "query": sql, "definition": sql,
            "refresh": 1, "multi": True, "includeAll": True,
            "current": {"selected": True, "text": ["All"], "value": ["$__all"]}, "sort": 0}

tableau = {
  "__inputs": [{"name": "DS_SUPABASE", "label": "Supabase (PostgreSQL)", "description": "Base Supabase du projet caravane",
                "type": "datasource", "pluginId": "grafana-postgresql-datasource", "pluginName": "PostgreSQL"}],
  "__requires": [{"type": "datasource", "id": "grafana-postgresql-datasource", "name": "PostgreSQL", "version": "1.0.0"},
                 {"type": "grafana", "id": "grafana", "name": "Grafana", "version": "11.0.0"}],
  "title": "Caravane — relevés", "uid": "caravane-releves",
  "description": "Relevés des cartes caravane (dépôt alphulpin/Caravane)", "tags": ["caravane"],
  "timezone": "browser", "editable": True, "graphTooltip": 1, "refresh": "5m",
  "time": {"from": "now-2d", "to": "now"}, "schemaVersion": 39, "version": 2,
  "templating": {"list": [variable("carte", "Carte", var_carte), variable("grandeur", "Grandeur", var_grandeur)]},
  "panels": panels,
  # Changements de configuration marqués sur les courbes
  "annotations": {"list": [{
    "name": "Configuration", "enable": True, "iconColor": "purple", "datasource": DS,
    "target": {"refId": "Anno", "editorMode": "code", "format": "table", "rawQuery": True, "rawSql": ANNOT}}]},
}
json.dump(tableau, open(os.path.join(ICI, "..", "grafana", "caravane-releves.json"), "w"), ensure_ascii=False, indent=2)
requetes = [t["rawSql"] for p in panels for t in p["targets"]] + [var_carte, var_grandeur, ANNOT]
json.dump(requetes, open("/tmp/requetes.json", "w"))
print(len(panels), "panneaux,", len(requetes), "requêtes")
