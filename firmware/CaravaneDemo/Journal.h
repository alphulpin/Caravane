// ==== Caravane =======================================================================
// Journal.h — firmware CaravaneDemo, version 0.9.2 — dernière modification : 2026-11-19
// Auteur : Alexandre Phulpin (développement assisté par Claude)
// Dépôt : https://github.com/alphulpin/Caravane
// ====
// Journal — messages de suivi sur le port série (moniteur série de l'IDE, 115200 bauds).
// Activé par défaut ; désactivable depuis le téléphone (Administration > Journal série) une fois
// la mise au point terminée : carte silencieuse en production.
// Gain : seulement le temps d'écriture (environ 1 ms par 100 caractères). Pas de gain de mémoire
// notable : les textes sont en flash, le formatage est temporaire, et les tampons du port série
// (1 à 2 Ko) restent réservés puisque le port est toujours initialisé.
#pragma once
#include <Arduino.h>

inline bool& journalActif() { static bool actif = true; return actif; }

#define journalf(...)  do { if (journalActif()) Serial.printf(__VA_ARGS__); } while (0)
#define journalln(...) do { if (journalActif()) Serial.println(__VA_ARGS__); } while (0)
#define journalp(...)  do { if (journalActif()) Serial.print(__VA_ARGS__); } while (0)
