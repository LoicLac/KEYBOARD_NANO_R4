#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <stdint.h>

// =================================================================
// 1. SYSTEME & DEBUG
// =================================================================
#define DEBUG_LEVEL 0

enum GameMode {
  MODE_PRESSURE_GLIDE,
  MODE_INTERVAL,
  MODE_MIDI
};

enum class UIEffect { NONE, VALIDATE, MODE_CHANGE_CHASE, MODE_CHANGE_CROSSFADE, MODE_CHANGE_INWARD_WIPE };
enum class ButtonID { HOLD, MODE, OCT_PLUS, OCT_MINUS };
enum class PotID { SENS }; // LIVE removed - replaced by encoder
enum class ButtonState { PRESS_SHORT, PRESS_LONG, RELEASED };


// =================================================================
// 2. BROCHAGE MATERIEL & CONSTANTES PHYSIQUES
// =================================================================
const int NUM_KEYS = 24;

const uint8_t ADDR_MPR121_A = 0x5A;
const uint8_t ADDR_MPR121_B = 0x5B;

#define PIN_POT_SENS  A3
// #define PIN_POT_LIVE  A4  // REMOVED - Replaced by rotary encoder

// Rotary Encoder Pins (replaces POT_LIVE)
#define PIN_ENCODER_A D11  // CLK pin
#define PIN_ENCODER_B D12  // DT pin
#define PIN_BTN_HOLD      D2  // TODO: Pinout is not ok - needs verification
#define PIN_BTN_MODE      D4
#define PIN_BTN_OCT_PLUS  D7
#define PIN_BTN_OCT_MINUS D8
#define PIN_CALIBRATION_BUTTON PIN_BTN_HOLD
#define PIN_GATE    A1
#define PIN_TRIGGER A2
#define PIN_LED_mm  D10
#define PIN_LED_m   D9
#define PIN_LED_c   D6
#define PIN_LED_p   D5
#define PIN_LED_pp  D3


// =================================================================
// 3. PARAMETRES TECHNIQUES DE BAS NIVEAU
// =================================================================
#define I2C_CLOCK_HZ 400000

const int BUTTON_DEBOUNCE_MS = 30;
const int MODE_BUTTON_LONG_PRESS_MS = 1000;
const int HOLD_BUTTON_LONG_PRESS_MS = 1000;
const int POT_DEADZONE = 4;

const float POT_SENS_SMOOTHING_ALPHA = 0.05f;
// POT_LIVE_SMOOTHING_ALPHA removed - encoder doesn't need smoothing

// Rotary Encoder Parameters
const int ENCODER_STEPS_PER_DETENT = 4;  // Typical encoder has 4 state changes per click
const unsigned long ENCODER_DEBOUNCE_TIME_MS = 2;  // Debounce time for encoder readings

#define DAC_I2C_ADDR 0x5F
#define CV_OUTPUT_RESOLUTION 4095


// =================================================================
// 4. CALIBRATION
// =================================================================
const uint16_t CAL_PRESSURE_MIN_DELTA_TO_VALIDATE = 300;
const uint16_t CAL_AUTOCONFIG_COUNTDOWN_MS = 1000;
const uint16_t BOOT_AUTOCONFIG_COUNTDOWN_MS = 1000;


// =================================================================
// 5. REPONSE MUSICALE & SENSATION DE JEU
// =================================================================
const float PRESS_THRESHOLD_PERCENT = 0.15f;
const float RELEASE_THRESHOLD_PERCENT = 0.08f;
const uint16_t MIN_PRESS_THRESHOLD = 20;
const uint16_t MIN_RELEASE_THRESHOLD = 10;

#define PITCH_STANDARD_VOLTS_PER_OCTAVE 1.0f
#define DAC_OUTPUT_VOLTAGE_RANGE 10.0f
#define PITCH_REFERENCE_MIDI_NOTE 47 
#define PITCH_CV_CENTER_VOLTAGE 5.0f
const float GLIDE_MAX_TIME_MS = 1000.0f;

#define MAX_OCTAVE 2
#define MIN_OCTAVE -2
const int TRIGGER_PULSE_DURATION_MS = 5;

#define AFTERTOUCH_CURVE_EXP_INTENSITY 4.0f
#define AFTERTOUCH_CURVE_SIG_INTENSITY 2

#define AFTERTOUCH_SMOOTHING_WINDOW_SIZE 4
#define AFTERTOUCH_SLEW_RATE_LIMIT 150

// -- ETAGE 2: Lissage Musical (dans EngineMode1) --
// Rôle: Créer des transitions douces de l'aftertouch entre deux notes jouées en legato.
// C'est le "Glide" de l'aftertouch. C'est ce paramètre qui a le plus d'impact sur la sensation de jeu legato.
#define AUX_VOLTAGE_SMOOTHING_ALPHA_DEFAULT 0.1f // La valeur par défaut au démarrage
#define AUX_SMOOTHING_MIN_ALPHA 0.001f           // Lissage max (temps très long), contrôlé par potard
#define AUX_SMOOTHING_MAX_ALPHA 0.9f             // Lissage min (quasi instantané), contrôlé par potard
// Technique: Coefficient du filtre passe-bas (0.0 à 1.0).
// Plage: 0.05 (très lent) à 0.3 (rapide).
// -> Diminuer (vers 0.0): La transition de pression entre deux notes sera très lente et douce.
// -> Augmenter (vers 1.0): La transition sera quasi instantanée et abrupte.

const int AFTERTOUCH_DEADZONE_MAX_OFFSET = 250;
// Technique: Valeur ADC maximale qui peut être ajoutée au seuil de départ de l'aftertouch.
// Musical: La "course à vide" de l'aftertouch après le déclenchement de la note.
// Plage: 0 (pas de zone morte) à ~200 (zone morte très prononcée).
// -> Augmenter: Nécessite une pression plus forte après le Note On pour que l'aftertouch commence à réagir.
// -> Diminuer: Rend l'aftertouch plus sensible et immédiat.

// La section 5.5 spécifique au Mode 2 a été déplacée dans EngineMode2.cpp

// --- 5.6 Comportement des LEDs en mode jeu --- (numérotation mise à jour)
const uint16_t LED_OCTAVE_BREATHE_PERIOD_MS = 2000;
const uint8_t  LED_OCTAVE_BREATHE_MIN_BRIGHTNESS = 20;

#endif  // HARDWARE_CONFIG_H