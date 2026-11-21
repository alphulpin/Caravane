// Décodeur TTN des trames de la carte caravane (The Things Stack > Application > Payload
// formatters > Uplink > Formatter type « Custom Javascript formatter » : coller ce fichier).
// Format décrit dans docs/donnees.md ; trame produite par trameReleve() (firmware, Lora.cpp).
//
// Port 1 : relevé. Sortie : { version, releve_id, secteur, valeurs: { temp_c, hum_pct, ... } }
// Port 2 : relevé suivi de la configuration de la carte (envoyée au démarrage, à chaque changement
//          et une fois par jour). Sortie : la même chose + config: { nom, firmware, ... }
// Le webhook Supabase (fonction caravane_ttn) range « valeurs » dans caravane_releves et
// « config » dans caravane_config.

function decodeUplink(input) {
  var o = input.bytes, i = 0, avert = [];
  function u8()  { return o[i++]; }
  function u16() { var v = o[i] | (o[i + 1] << 8); i += 2; return v; }
  function s16() { var v = u16(); return v > 32767 ? v - 65536 : v; }
  function u32() { return (u16() + u16() * 65536) >>> 0; }

  if (input.fPort !== 1 && input.fPort !== 2) return { data: {}, warnings: ["port " + input.fPort + " non géré"] };
  if (o.length < 7) return { errors: ["trame trop courte (" + o.length + " octets)"] };
  var version = u8();
  if (version !== 1) return { errors: ["version de trame inconnue : " + version] };

  var releve = u32();
  var drapeaux = u8();
  var masque = u8();
  var v = {};
  try {
    if (masque & 0x01) v.temp_c    = s16() / 10;
    if (masque & 0x02) v.hum_pct   = u8() / 2;
    if (masque & 0x04) v.press_hpa = Math.round((u16() / 10 + 500) * 10) / 10;
    if (masque & 0x08) v.eco2_ppm  = u16();
    if (masque & 0x10) v.tvoc_ppb  = u16();
    if (masque & 0x20) v.vbat_mv   = u8() * 10 + 2500;
    if (masque & 0x40) v.bat12_mv  = u16();
    if (masque & 0x80) avert.push("2e octet de masque non géré");
  } catch (e) {
    return { errors: ["trame tronquée"] };
  }
  if (i > o.length) return { errors: ["trame tronquée"] };
  var data = { version: version, releve_id: releve, secteur: (drapeaux & 0x01) === 1, valeurs: v };

  if (input.fPort === 2) {
    if (o.length < i + 14) return { errors: ["bloc de configuration tronqué"] };
    var EVT = ["demarrage", "changement", "periodique"];
    var CAUSE = ["autre", "mise sous tension", "redémarrage demandé", "plantage", "chien de garde",
                 "baisse de tension", "réveil", "bouton reset"];
    var COMM = ["aucun", "wifi", "lora", "wifi_lora"];
    var evt = u8(), cause = u8();
    var fw = u8() + "." + u8() + "." + u8();
    var comm = u8(), releveMin = u16(), veilleMin = u16();
    var puce = u8(); if (puce > 127) puce -= 256;
    var fl = u8(), l = u8(), nom = "";
    if (o.length < i + l) return { errors: ["nom tronqué"] };
    var octets = o.slice(i, i + l); i += l;
    try { nom = decodeURIComponent(escape(String.fromCharCode.apply(null, octets))); }   // UTF-8
    catch (e) { nom = String.fromCharCode.apply(null, octets); }
    var c = { nom: nom, firmware: fw, comm: COMM[comm] || String(comm),
              releve_s: releveMin * 60, veille_s: veilleMin * 60, journal: (fl & 0x01) === 1 };
    if (evt === 0) c.cause = CAUSE[cause] || "autre";
    data.evenement = EVT[evt] || "periodique";
    data.config = c;
    data.etat = { secteur: data.secteur };
    if (puce !== 127) data.etat.temp_puce_c = puce;
    if (v.vbat_mv) data.etat.vbat_mv = v.vbat_mv;
  }
  return { data: data, warnings: avert };
}
