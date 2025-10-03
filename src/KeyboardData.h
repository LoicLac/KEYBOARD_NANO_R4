#ifndef KEYBOARD_DATA_H
#define KEYBOARD_DATA_H

#include <stdint.h>
#include "HardwareConfig.h" // Fournit la constante NUM_KEYS

// =================================================================
// Constantes et Structures de Données Musicales
// =================================================================

// Taille de la pile de notes pour les moteurs de jeu.
const int NOTE_STACK_SIZE = 16;

// Structure représentant une note active dans la pile.
struct Note {
  uint8_t pitch;
  uint16_t value; // Pression ou vélocité
};

// =================================================================
// Constantes et Structure de Données de Calibration
// =================================================================

// --- Constantes EEPROM ---
const uint16_t EEPROM_MAGIC   = 0xBEEF;
const uint8_t  EEPROM_VERSION = 3;

// --- Structure de stockage ---
struct CalDataStore {
  uint16_t magic;
  uint8_t  version;
  uint8_t  reserved;
  uint16_t target_baseline;
  uint16_t maxDelta[NUM_KEYS]; // Utilise NUM_KEYS défini dans HardwareConfig.h
};


#endif // KEYBOARD_DATA_H