// Décodeur TTN des trames de la carte caravane (The Things Stack > Application > Payload
// formatters > Uplink > Formatter type « Custom Javascript formatter » : coller ce fichier).
// Format décrit dans docs/donnees.md ; trame produite par trameReleve() (firmware, Lora.cpp).
//
// Port 1 : relevé. Sortie : { version, releve_id, secteur, valeurs: { temp_c, hum_pct, ... } }
// Le webhook Supabase (fonction caravane_ttn) range « valeurs » tel quel dans caravane_releves.

function decodeUplink(input) {
  var o = input.bytes, i = 0, avert = [];
  function u8()  { return o[i++]; }
  function u16() { var v = o[i] | (o[i + 1] << 8); i += 2; return v; }
  function s16() { var v = u16(); return v > 32767 ? v - 65536 : v; }
  function u32() { return (u16() + u16() * 65536) >>> 0; }

  if (input.fPort !== 1) return { data: {}, warnings: ["port " + input.fPort + " non géré"] };
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
  return {
    data: { version: version, releve_id: releve, secteur: (drapeaux & 0x01) === 1, valeurs: v },
    warnings: avert
  };
}
