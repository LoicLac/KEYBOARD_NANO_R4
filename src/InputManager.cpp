#include "InputManager.h"
#include <Arduino.h> 

InputManager::InputManager() :
  _btnHold(PIN_BTN_HOLD, BUTTON_DEBOUNCE_MS),
  _btnMode(PIN_BTN_MODE, BUTTON_DEBOUNCE_MS),
  _btnOctPlus(PIN_BTN_OCT_PLUS, BUTTON_DEBOUNCE_MS),
  _btnOctMinus(PIN_BTN_OCT_MINUS, BUTTON_DEBOUNCE_MS),
  _liveEncoder(PIN_ENCODER_A, PIN_ENCODER_B, ENCODER_DEBOUNCE_TIME_MS)  // Initialize encoder with debounce
{
  _smoothedPotSens = 0.0f;
  _lastPotSensSent = -POT_DEADZONE * 2;
  _holdLongPressTriggered = false;
  _modeLongPressTriggered = false;
  _octPlus_comboHappened = false;
  _octMinus_comboHappened = false;
}

void InputManager::begin() {
  _btnHold.begin();
  _btnMode.begin();
  _btnOctPlus.begin();
  _btnOctMinus.begin();
}

const InputEvents& InputManager::getEvents() const {
  return _events;
}

bool InputManager::isHoldPressedOnBoot() {
  pinMode(PIN_BTN_HOLD, INPUT_PULLUP);
  return digitalRead(PIN_BTN_HOLD) == LOW;
}

void InputManager::update() {
  // --- Etape 1: Réinitialiser les flags ---
  _events.hold_wasPressedShort = false;
  _events.hold_wasPressedLong = false;
  _events.mode_wasPressedShort = false;
  _events.mode_wasPressedLong = false;
  _events.octPlus_wasReleasedAsShort = false;
  _events.octMinus_wasReleasedAsShort = false;
  _events.sens_potMoved = false;
  _events.live_encoderTurned = false;
  _events.combo_OctPlus_LiveMoved = false;
  _events.combo_OctMinus_LiveMoved = false;

  // --- Etape 2: Lire les boutons ---
  _btnHold.read();
  _btnMode.read();
  _btnOctPlus.read();
  _btnOctMinus.read();

  // --- Etape 3: Exposer l'état des "Shift" ---
  _events.octPlus_isPressed = _btnOctPlus.isPressed();
  _events.octMinus_isPressed = _btnOctMinus.isPressed();

  // --- Etape 4: Lire les contrôleurs (potentiomètre + encodeur) ---
  
  // Keep pot_sens logic unchanged
  _smoothedPotSens = (POT_SENS_SMOOTHING_ALPHA * analogRead(PIN_POT_SENS)) + (1.0f - POT_SENS_SMOOTHING_ALPHA) * _smoothedPotSens;

  if (abs((int)_smoothedPotSens - _lastPotSensSent) > POT_DEADZONE) {
    _events.sens_potMoved = true;
    _events.potSensValue = (int)_smoothedPotSens;
    _lastPotSensSent = _events.potSensValue;
  } else {
    _events.potSensValue = _lastPotSensSent;
  }

  // Replace pot_live with encoder reading
  int encoderDelta = _liveEncoder.read();
  if (encoderDelta != 0) {
    _events.live_encoderTurned = true;
    _events.live_encoderDelta = encoderDelta;
  } else {
    _events.live_encoderTurned = false;
    _events.live_encoderDelta = 0;
  }
  
  // --- Etape 5: Détecter les événements de boutons et combinaisons ---
  if (_btnHold.isPressed()) {
    if (_btnHold.pressedFor(HOLD_BUTTON_LONG_PRESS_MS) && !_holdLongPressTriggered) {
      _events.hold_wasPressedLong = true;
      _holdLongPressTriggered = true;
    }
  } else if (_btnHold.wasReleased()) {
    if (!_holdLongPressTriggered) {
      _events.hold_wasPressedShort = true;
    }
    _holdLongPressTriggered = false;
  }

  if (_btnMode.isPressed()) {
    if (_btnMode.pressedFor(MODE_BUTTON_LONG_PRESS_MS) && !_modeLongPressTriggered) {
      _events.mode_wasPressedLong = true;
      _modeLongPressTriggered = true;
    }
  } else if (_btnMode.wasReleased()) {
    if (!_modeLongPressTriggered) {
      _events.mode_wasPressedShort = true;
    }
    _modeLongPressTriggered = false;
  }

  // --- LOGIQUE "ACTION ON RELEASE" CORRIGÉE POUR OCTAVE ---
  if (_events.octPlus_isPressed) {
    if (_events.live_encoderTurned) {  // CHANGED: was live_potMoved
      _events.combo_OctPlus_LiveMoved = true;
      _octPlus_comboHappened = true; // On mémorise qu'un combo a eu lieu
    }
  } else if (_btnOctPlus.wasReleased()) {
    if (!_octPlus_comboHappened) {
      // S'il n'y a pas eu de combo, c'était un appui court
      _events.octPlus_wasReleasedAsShort = true;
    }
    _octPlus_comboHappened = false; // On réinitialise l'état au relâchement
  }

  if (_events.octMinus_isPressed) {
    if (_events.live_encoderTurned) {  // CHANGED: was live_potMoved
      _events.combo_OctMinus_LiveMoved = true;
      _octMinus_comboHappened = true;
    }
  } else if (_btnOctMinus.wasReleased()) {
    if (!_octMinus_comboHappened) {
      _events.octMinus_wasReleasedAsShort = true;
    }
    _octMinus_comboHappened = false;
  }
}