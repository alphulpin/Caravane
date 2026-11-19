// ==== Caravane =======================================================================
// Journal.h — firmware CaravaneDemo, version 0.9.2 — dernière modification : 2026-11-19
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Journal — messages de suivi sur le port série (moniteur série de l'IDE, 115200 bauds).
// Activé par défaut ; désactivable depuis le téléphone (Administration > Journal série) une fois
// la mise au point terminée. Le code reste dans le firmware : on évite seulement le temps passé
// à écrire (environ 1 ms par 100 caractères à 115200 bauds).
#pragma once
#include <Arduino.h>

inline bool& journalActif() { static bool actif = true; return actif; }

#define journalf(...)  do { if (journalActif()) Serial.printf(__VA_ARGS__); } while (0)
#define journalln(...) do { if (journalActif()) Serial.println(__VA_ARGS__); } while (0)
#define journalp(...)  do { if (journalActif()) Serial.print(__VA_ARGS__); } while (0)
