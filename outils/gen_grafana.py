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
    p.update(autres)
    return p

ts_def = {"custom": {"lineWidth": 2, "fillOpacity": 8, "showPoints": "auto", "spanNulls": 3600000,
                     "drawStyle": "line", "lineInterpolation": "smooth"}}
ts_opts = {"legend": {"displayMode": "list", "placement": "bottom", "showLegend": True},
           "tooltip": {"mode": "multi", "sort": "none"}}
alim = {"mappings": [{"type": "value", "options": {"0": {"text": "Batterie", "color": "orange"},
                                                   "1": {"text": "Secteur", "color": "green"}}}]}
T = "coalesce(mesure_le, recu_le)"

panels = [
  # Dernière valeur de chaque grandeur choisie, pour chaque carte choisie
  panneau("stat", "Dernières valeurs", 0, 0, 18, 5,
          [cible("select distinct on (appareil, grandeur)\n"
                 "  nom || ' · ' || libelle || coalesce(' (' || unite || ')', '') as \"grandeur\", valeur\n"
                 "from caravane_series\n"
                 "where appareil in ($carte) and grandeur in ($grandeur)\n"
                 "order by appareil, grandeur, mesure_le desc", "table")],
          {"decimals": 1, "color": {"mode": "fixed", "fixedColor": "blue"}},
          {"reduceOptions": {"calcs": ["lastNotNull"], "fields": "/^valeur$/", "values": True},
           "colorMode": "value", "graphMode": "none", "textMode": "value_and_name", "justifyMode": "auto",
           "orientation": "auto", "showPercentChange": False}),
  panneau("stat", "Dernier relevé", 18, 0, 6, 5,
          [cible("select coalesce(nom, appareil) as carte, extract(epoch from max(" + T + ")) * 1000 as \"dernier relevé\"\n"
                 "from caravane_releves\nwhere appareil in ($carte)\ngroup by 1", "table")],
          {"unit": "dateTimeFromNow"},
          {"reduceOptions": {"calcs": ["lastNotNull"], "fields": "", "values": True},
           "colorMode": "none", "graphMode": "none", "textMode": "value_and_name"}),
  # Une courbe par grandeur choisie (panneau répété), une série par carte
  panneau("timeseries", "${grandeur:text}", 0, 5, 12, 8,
          [cible("select mesure_le as time, nom as metric, valeur as value\n"
                 "from caravane_series\n"
                 "where appareil in ($carte) and grandeur = '$grandeur' and $__timeFilter(mesure_le)\n"
                 "order by 1")],
          ts_def, ts_opts, repeat="grandeur", repeatDirection="h", maxPerRow=2),
  panneau("state-timeline", "Alimentation", 0, 13, 24, 4,
          [cible("select " + T + " as time, coalesce(nom, appareil) as metric,\n"
                 "  case when secteur then 1 else 0 end as value\n"
                 "from caravane_releves\n"
                 "where appareil in ($carte) and secteur is not null and $__timeFilter(" + T + ")\norder by 1")],
          {**alim, "custom": {"fillOpacity": 80, "lineWidth": 0}},
          {"mergeValues": True, "showValue": "never", "alignValue": "left", "rowHeight": 0.8,
           "legend": {"showLegend": False}}),
  panneau("table", "Derniers relevés", 0, 17, 24, 9,
          [cible("select " + T + " as \"heure\", coalesce(nom, appareil) as \"carte\", releve_id as \"n°\", source,\n"
                 "  case when secteur then 'secteur' else 'batterie' end as \"alim.\",\n"
                 "  valeurs::text as \"valeurs\", recu_le as \"reçu le\"\n"
                 "from caravane_releves\nwhere appareil in ($carte) and $__timeFilter(" + T + ")\n"
                 "order by 1 desc\nlimit 200", "table")],
          None, {"showHeader": True, "cellHeight": "sm", "footer": {"show": False}}),
]

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
}
json.dump(tableau, open(os.path.join(ICI, "..", "grafana", "caravane-releves.json"), "w"), ensure_ascii=False, indent=2)
requetes = [t["rawSql"] for p in panels for t in p["targets"]] + [var_carte, var_grandeur]
json.dump(requetes, open("/tmp/requetes.json", "w"))
print(len(panels), "panneaux,", len(requetes), "requêtes")
