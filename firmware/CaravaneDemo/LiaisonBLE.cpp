#include "LiaisonBLE.h"
#include "Config.h"
#include <BLEUtils.h>
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <BLE2902.h>
#endif

static const size_t TAMPON_MAX = 1024;   // une ligne de réglage Supabase peut dépasser 512 octets
static const size_t TAILLE_NOTIFICATION = 20;   // sûr quel que soit le MTU négocié

void LiaisonBLE::demarrer(const char* nom) {
  _verrou = xSemaphoreCreateMutex();
  BLEDevice::init(nom);
  _serveur = BLEDevice::createServer();
  _serveur->setCallbacks(this);

  BLEService* service = _serveur->createService(BLE_UUID_SERVICE);
  _tx = service->createCharacteristic(BLE_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
#if defined(CONFIG_BLUEDROID_ENABLED)
  _tx->addDescriptor(new BLE2902());            // NimBLE l'ajoute tout seul
#endif
  BLECharacteristic* rx = service->createCharacteristic(
    BLE_UUID_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(this);
  service->start();

  BLEAdvertising* annonce = BLEDevice::getAdvertising();
  annonce->addServiceUUID(BLE_UUID_SERVICE);    // permet le filtrage côté Web Bluetooth
  annonce->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void LiaisonBLE::onConnect(BLEServer*) {
  _connecte = true;
  _activite = true;
  _nouvelle = true;
}

void LiaisonBLE::onDisconnect(BLEServer*) {
  _connecte = false;
  _activite = true;
  _relancerAnnonce = true;
}

void LiaisonBLE::onWrite(BLECharacteristic* c) {
  String recu = c->getValue();
  if (recu.length() == 0) return;
  xSemaphoreTake(_verrou, portMAX_DELAY);
  if (_tampon.length() + recu.length() > TAMPON_MAX) _tampon = "";   // ligne trop longue : on repart à zéro
  _tampon += recu;
  xSemaphoreGive(_verrou);
  _activite = true;
}

bool LiaisonBLE::lireLigne(String& ligne) {
  bool trouve = false;
  xSemaphoreTake(_verrou, portMAX_DELAY);
  int fin = _tampon.indexOf('\n');
  if (fin >= 0) {
    ligne = _tampon.substring(0, fin);
    _tampon.remove(0, fin + 1);
    trouve = true;
  }
  xSemaphoreGive(_verrou);
  if (trouve) ligne.trim();
  return trouve;
}

void LiaisonBLE::envoyerLigne(const String& ligne) {
  if (!_connecte || _tx == nullptr) return;
  String trame = ligne + "\n";
  for (size_t i = 0; i < trame.length(); i += TAILLE_NOTIFICATION) {
    size_t reste = trame.length() - i;
    size_t n = reste < TAILLE_NOTIFICATION ? reste : TAILLE_NOTIFICATION;
    _tx->setValue((const uint8_t*)trame.c_str() + i, n);
    _tx->notify();
    delay(15);                                  // laisse la pile BLE écouler le morceau
  }
}

bool LiaisonBLE::activiteDepuisDernierAppel() {
  bool a = _activite;
  _activite = false;
  return a;
}

bool LiaisonBLE::nouvelleConnexion() {
  bool n = _nouvelle;
  _nouvelle = false;
  return n;
}

void LiaisonBLE::entretenir() {
  if (_relancerAnnonce) {
    _relancerAnnonce = false;
    delay(300);
    BLEDevice::startAdvertising();
  }
}
