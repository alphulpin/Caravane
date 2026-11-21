// ==== Caravane =======================================================================
// Environnement.cpp — firmware CaravaneDemo, version 0.10.2 — dernière modification : 2026-11-19
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
#include "Environnement.h"
#include "Journal.h"

static const uint8_t ADR_HDC = 0x40;
static const uint8_t ADR_BMP = 0x76;

// ---------- Accès I2C ----------
bool Environnement::present(uint8_t adr) {
  _bus->beginTransmission(adr);
  return _bus->endTransmission() == 0;
}

bool Environnement::ecrire(uint8_t adr, uint8_t reg, const uint8_t* data, size_t n) {
  _bus->beginTransmission(adr);
  _bus->write(reg);
  for (size_t i = 0; i < n; i++) _bus->write(data[i]);
  return _bus->endTransmission() == 0;
}

bool Environnement::lireRegs(uint8_t adr, uint8_t reg, uint8_t* data, size_t n) {
  _bus->beginTransmission(adr);
  _bus->write(reg);
  if (_bus->endTransmission(false) != 0) return false;
  if (_bus->requestFrom(adr, (uint8_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) data[i] = _bus->read();
  return true;
}

// HDC1080 : lecture d'un registre avec un STOP entre l'écriture du pointeur et la lecture
// (il ne tolère pas toujours le « repeated start » utilisé pour les autres capteurs).
static bool lireHdcRegistre(TwoWire* bus, uint8_t reg, uint8_t* data, size_t n, uint16_t attenteMs) {
  bus->beginTransmission(ADR_HDC);
  bus->write(reg);
  if (bus->endTransmission(true) != 0) return false;
  delay(attenteMs);
  if (bus->requestFrom(ADR_HDC, (uint8_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) data[i] = bus->read();
  return true;
}

// ---------- Démarrage ----------
void Environnement::demarrer(uint8_t sda, uint8_t scl, bool partage) {
  if (partage) {
    _bus = &Wire;                    // bus de l'écran (sans effet s'il est déjà démarré)
    _bus->begin(sda, scl, 100000);
  } else {
    _bus = &Wire1;
    _bus->begin(sda, scl, 100000);   // 100 kHz : le CCS811 étire l'horloge
  }
  _bus->setTimeOut(100);
  delay(20);                         // laisse les capteurs s'éveiller après Ve

  // Température/humidité à 0x40. La CJMCU-8128 est censée porter un HDC1080, mais certains
  // modules ont un Si7021/HTU21D à la même adresse, avec d'autres commandes : on teste les deux.
  _typeTH = TypeTH::Aucun;
  uint8_t id[2] = {}, dev[2] = {};
  bool fab = lireHdcRegistre(_bus, 0xFE, id, 2, 2);
  bool comp = lireHdcRegistre(_bus, 0xFF, dev, 2, 2);
  _idTH[0] = id[0]; _idTH[1] = id[1]; _idTH[2] = dev[0]; _idTH[3] = dev[1];
  if ((fab && id[0] == 0x54 && id[1] == 0x49) || (comp && dev[0] == 0x10 && dev[1] == 0x50)) {
    _typeTH = TypeTH::Hdc1080;
    const uint8_t cfg[2] = { 0x10, 0x00 };   // température + humidité à la suite, 14 bits
    ecrire(ADR_HDC, 0x02, cfg, 2);
    delay(2);
  } else if (present(ADR_HDC)) {
    // Si7021/HTU21D : 0xFE (lu plus haut) est sa commande de réinitialisation ; on attend
    // puis on lit son registre utilisateur (0xE7).
    delay(20);
    uint8_t ur = 0;
    if (lireHdcRegistre(_bus, 0xE7, &ur, 1, 1)) {
      _typeTH = TypeTH::Si7021;
      _idTH[3] = ur;
    }
  }
  _m.hdcOk = _typeTH != TypeTH::Aucun;

  _m.bmpOk = initBmp();

  // CCS811 : identifiant matériel 0x81, à l'adresse 0x5A ou 0x5B
  _adrCcs = 0;
  for (uint8_t adr : { (uint8_t)0x5A, (uint8_t)0x5B }) {
    uint8_t hw;
    if (lireRegs(adr, 0x20, &hw, 1) && hw == 0x81) { _adrCcs = adr; break; }
  }
  _m.ccsOk = _adrCcs != 0;
  _ccsActif = false;
  _brut = _m;

  journalf("Capteurs : temp./hum. %s (id %02X%02X/%02X%02X), BMP280 %s, CCS811 %s\n",
                nomTH(), _idTH[0], _idTH[1], _idTH[2], _idTH[3], _m.bmpOk ? "ok" : "absent",
                _m.ccsOk ? (_adrCcs == 0x5A ? "ok (0x5A)" : "ok (0x5B)") : "absent");
}

void Environnement::balayer() {
  journalf("Balayage I2C (%s) :", _bus == &Wire ? "Wire, partagé avec l'écran" : "Wire1");
  _nbAdresses = 0;
  for (uint8_t adr = 1; adr < 127; adr++) {
    if (!present(adr)) continue;
    journalf(" 0x%02X", adr);
    if (_nbAdresses < sizeof(_adresses)) _adresses[_nbAdresses++] = adr;
  }
  journalln(_nbAdresses ? "" : " aucun périphérique");
}

const char* Environnement::nomTH() const {
  switch (_typeTH) {
    case TypeTH::Hdc1080: return "HDC1080";
    case TypeTH::Si7021:  return "Si7021";
    default:              return "absent";
  }
}

String Environnement::diagnostic() const {
  char t[96];
  snprintf(t, sizeof(t), "Temp./hum. : %s, id %02X%02X/%02X%02X, brut %02X%02X %02X%02X -> %.1f C %.0f %%",
           nomTH(), _idTH[0], _idTH[1], _idTH[2], _idTH[3], _brutTH[0], _brutTH[1], _brutTH[2], _brutTH[3],
           _m.temperatureC, _m.humiditePct);
  return t;
}

// ---------- HDC1080 ----------
void Environnement::lireHdc() {
  if (_typeTH == TypeTH::Si7021) { lireSi7021(); return; }
  if (_typeTH != TypeTH::Hdc1080) return;
  uint8_t b[4];                       // pointeur 0x00 : déclenche température puis humidité
  if (!lireHdcRegistre(_bus, 0x00, b, 4, 20)) {
    journalln("HDC1080 : lecture impossible");
    return;
  }
  memcpy(_brutTH, b, 4);
  uint16_t rt = (b[0] << 8) | b[1], rh = (b[2] << 8) | b[3];
  _m.temperatureC = rt / 65536.0f * 165.0f - 40.0f;
  _m.humiditePct  = rh / 65536.0f * 100.0f;
}

// ---------- Si7021 / HTU21D ----------
void Environnement::lireSi7021() {
  uint8_t t[3], h[3];                 // commandes « sans maintien » : 0xF3 température, 0xF5 humidité
  if (!lireHdcRegistre(_bus, 0xF3, t, 3, 60) || !lireHdcRegistre(_bus, 0xF5, h, 3, 30)) {
    journalln("Si7021 : lecture impossible");
    return;
  }
  _brutTH[0] = t[0]; _brutTH[1] = t[1]; _brutTH[2] = h[0]; _brutTH[3] = h[1];
  uint16_t rt = ((t[0] << 8) | t[1]) & 0xFFFC, rh = ((h[0] << 8) | h[1]) & 0xFFFC;
  _m.temperatureC = -46.85f + 175.72f * rt / 65536.0f;
  _m.humiditePct  = constrain(-6.0f + 125.0f * rh / 65536.0f, 0.0f, 100.0f);
}

// ---------- BMP280 ----------
bool Environnement::initBmp() {
  uint8_t id;
  if (!lireRegs(ADR_BMP, 0xD0, &id, 1) || id != 0x58) return false;
  uint8_t c[24];
  if (!lireRegs(ADR_BMP, 0x88, c, 24)) return false;
  auto u16 = [&](int i) { return (uint16_t)(c[i] | (c[i + 1] << 8)); };
  auto s16 = [&](int i) { return (int16_t)(c[i] | (c[i + 1] << 8)); };
  _T1 = u16(0);  _T2 = s16(2);  _T3 = s16(4);
  _P1 = u16(6);  _P2 = s16(8);  _P3 = s16(10); _P4 = s16(12); _P5 = s16(14);
  _P6 = s16(16); _P7 = s16(18); _P8 = s16(20); _P9 = s16(22);
  const uint8_t config = 0x00;       // pas de filtre
  ecrire(ADR_BMP, 0xF5, &config, 1);
  return true;
}

void Environnement::lireBmp() {
  if (!_m.bmpOk) return;
  const uint8_t forcee = 0x2D;       // température x1, pression x4, mesure unique
  if (!ecrire(ADR_BMP, 0xF4, &forcee, 1)) return;
  delay(20);
  uint8_t d[6];
  if (!lireRegs(ADR_BMP, 0xF7, d, 6)) return;
  int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);

  // Compensation : formules entières de la fiche technique Bosch
  int32_t v1 = ((((adcT >> 3) - ((int32_t)_T1 << 1))) * (int32_t)_T2) >> 11;
  int32_t v2 = (((((adcT >> 4) - (int32_t)_T1) * ((adcT >> 4) - (int32_t)_T1)) >> 12) * (int32_t)_T3) >> 14;
  int32_t tFine = v1 + v2;

  int64_t a = (int64_t)tFine - 128000;
  int64_t b = a * a * (int64_t)_P6;
  b += (a * (int64_t)_P5) << 17;
  b += ((int64_t)_P4) << 35;
  a = ((a * a * (int64_t)_P3) >> 8) + ((a * (int64_t)_P2) << 12);
  a = ((((int64_t)1) << 47) + a) * ((int64_t)_P1) >> 33;
  if (a == 0) return;
  int64_t p = 1048576 - adcP;
  p = (((p << 31) - b) * 3125) / a;
  a = (((int64_t)_P9) * (p >> 13) * (p >> 13)) >> 25;
  b = (((int64_t)_P8) * p) >> 19;
  p = ((p + a + b) >> 8) + (((int64_t)_P7) << 4);
  _m.pressionHpa = (float)p / 256.0f / 100.0f;
}

// ---------- CCS811 ----------
void Environnement::activerQualiteAir(bool actif) {
  if (!_m.ccsOk || actif == _ccsActif) return;
  if (actif) {
    uint8_t etat;
    if (!lireRegs(_adrCcs, 0x00, &etat, 1) || !(etat & 0x10)) return;  // pas d'application valide
    if (!(etat & 0x80)) {                         // encore en mode démarrage : lancer l'appli
      _bus->beginTransmission(_adrCcs);
      _bus->write(0xF4);
      if (_bus->endTransmission() != 0) return;
      delay(5);
    }
    const uint8_t mode = 0x10;                    // une mesure par seconde
    if (!ecrire(_adrCcs, 0x01, &mode, 1)) return;
    _ccsActif = true;
    journalln("Qualité d'air : démarrée");
  } else {
    const uint8_t repos = 0x00;
    ecrire(_adrCcs, 0x01, &repos, 1);
    _ccsActif = false;
    _m.eco2Ppm = _m.tvocPpb = 0;
    _brut.eco2Ppm = _brut.tvocPpb = 0;
    journalln("Qualité d'air : arrêtée");
  }
}

void Environnement::lireCcs() {
  if (!_ccsActif) return;
  // Compensation par l'humidité et la température du HDC1080
  if (!isnan(_m.humiditePct) && !isnan(_m.temperatureC)) {
    uint16_t h = (uint16_t)(_m.humiditePct * 512.0f);
    uint16_t t = (uint16_t)((_m.temperatureC + 25.0f) * 512.0f);
    const uint8_t env[4] = { (uint8_t)(h >> 8), (uint8_t)h, (uint8_t)(t >> 8), (uint8_t)t };
    ecrire(_adrCcs, 0x05, env, 4);
  }
  uint8_t r[6];
  if (!lireRegs(_adrCcs, 0x02, r, 6)) return;
  if (r[4] & 0x01) { journalf("CCS811 erreur 0x%02X\n", r[5]); return; }
  if (!(r[4] & 0x08)) return;                     // pas de nouvelle valeur
  _m.eco2Ppm = (r[0] << 8) | r[1];
  _m.tvocPpb = (r[2] << 8) | r[3];
}

void Environnement::lire() {
  // Les pilotes gardent la valeur précédente quand rien de neuf n'arrive (CCS811) : ils
  // travaillent donc sur les valeurs brutes, et l'étalonnage est appliqué à une copie.
  _m = _brut;
  lireHdc();
  lireBmp();
  lireCcs();                                         // compensée avec la température brute
  _brut = _m;
  if (!_etal) return;
  _m.temperatureC = _etal->appliquer(G_TEMP, _m.temperatureC);
  if (!isnan(_m.humiditePct)) _m.humiditePct = constrain(_etal->appliquer(G_HUM, _m.humiditePct), 0.0f, 100.0f);
  _m.pressionHpa = _etal->appliquer(G_PRESS, _m.pressionHpa);
  auto entier = [&](uint8_t g, uint16_t v) -> uint16_t {
    if (v == 0) return 0;                            // 0 = pas de valeur
    return (uint16_t)constrain(lroundf(_etal->appliquer(g, v)), 1L, 65535L);
  };
  _m.eco2Ppm = entier(G_ECO2, _m.eco2Ppm);
  _m.tvocPpb = entier(G_TVOC, _m.tvocPpb);
}
