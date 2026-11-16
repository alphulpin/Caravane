// LiaisonBLE — périphérique BLE "Nordic UART" : lignes de texte dans les deux sens.
// Les callbacks BLE tournent dans une autre tâche : ils ne font que déposer les
// données, tout le traitement se fait dans loop() via lireLigne().
#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>

class LiaisonBLE : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
  void demarrer(const char* nom);
  bool connecte() const { return _connecte; }
  bool lireLigne(String& ligne);      // true si une ligne complète ('\n') est arrivée
  void envoyerLigne(const String& ligne);
  bool activiteDepuisDernierAppel();  // connexion, déconnexion ou réception
  bool nouvelleConnexion();           // true une seule fois après chaque connexion
  void entretenir();                  // à appeler dans loop() : relance l'annonce

private:
  void onConnect(BLEServer* serveur) override;
  void onDisconnect(BLEServer* serveur) override;
  void onWrite(BLECharacteristic* caracteristique) override;

  BLEServer* _serveur = nullptr;
  BLECharacteristic* _tx = nullptr;
  SemaphoreHandle_t _verrou = nullptr;
  String _tampon;                     // octets reçus, protégés par _verrou
  volatile bool _connecte = false;
  volatile bool _activite = false;
  volatile bool _nouvelle = false;
  volatile bool _relancerAnnonce = false;
};
