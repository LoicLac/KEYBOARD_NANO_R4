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
  _octPlus_longPressTriggered = false;
  _octMinus_longPressTriggered = false;
  
  _encoderLastTurnTime = 0;
  _encoderVelocity = 0.0f;
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
  _events.octPlus_isLongPressed = false;
  _events.octMinus_isLongPressed = false;

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

  // Replace pot_live with encoder reading and velocity tracking
  int encoderDelta = _liveEncoder.read();
  unsigned long now = millis();
  
  if (encoderDelta != 0) {
    unsigned long timeSinceLastTurn = now - _encoderLastTurnTime;
    
    // Calculate instantaneous velocity
    float instantVelocity = 0.0f;
    if (timeSinceLastTurn > 0 && timeSinceLastTurn < ENCODER_VELOCITY_WINDOW_MS * 3) {
      // Convert to ticks per ENCODER_VELOCITY_WINDOW_MS
      instantVelocity = (abs(encoderDelta) * ENCODER_VELOCITY_WINDOW_MS) / (float)timeSinceLastTurn;
      instantVelocity = constrain(instantVelocity, 0.0f, (float)ENCODER_VELOCITY_MAX);
    } else {
      // Very slow or first turn - use base velocity
      instantVelocity = abs(encoderDelta);
    }
    
    // Apply smoothing to velocity (reduces jitter)
    _encoderVelocity = ENCODER_VELOCITY_SMOOTHING * instantVelocity + 
                       (1.0f - ENCODER_VELOCITY_SMOOTHING) * _encoderVelocity;
    
    _encoderLastTurnTime = now;
    _events.live_encoderTurned = true;
    _events.live_encoderDelta = encoderDelta;
    _events.live_encoderVelocity = (int)_encoderVelocity;
  } else {
    // Decay velocity when encoder stops
    unsigned long timeSinceLastTurn = now - _encoderLastTurnTime;
    if (timeSinceLastTurn > ENCODER_VELOCITY_WINDOW_MS * 2) {
      _encoderVelocity *= 0.9f;  // Exponential decay
      if (_encoderVelocity < 0.1f) _encoderVelocity = 0.0f;
    }
    
    _events.live_encoderTurned = false;
    _events.live_encoderDelta = 0;
    _events.live_encoderVelocity = (int)_encoderVelocity;
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

  // --- Oct+ and Oct- Button Logic ---
  // Oct+ button logic with 500ms long press requirement
  if (_btnOctPlus.isPressed()) {
    if (_btnOctPlus.pressedFor(500) && !_octPlus_longPressTriggered) {
      _octPlus_longPressTriggered = true;
    }
    // Keep flag true while button is held past 500ms
    if (_octPlus_longPressTriggered) {
      _events.octPlus_isLongPressed = true;
    }
    if (_events.octPlus_isLongPressed && _events.live_encoderTurned) {
      _events.combo_OctPlus_LiveMoved = true;
      _octPlus_comboHappened = true;
    }
  } else if (_btnOctPlus.wasReleased()) {
    if (!_octPlus_comboHappened && !_octPlus_longPressTriggered) {
      // Short press without combo and without long press
      _events.octPlus_wasReleasedAsShort = true;
    }
    _octPlus_comboHappened = false;
    _octPlus_longPressTriggered = false;
  }

  // Oct- button logic with 500ms long press requirement
  if (_btnOctMinus.isPressed()) {
    if (_btnOctMinus.pressedFor(500) && !_octMinus_longPressTriggered) {
      _octMinus_longPressTriggered = true;
    }
    // Keep flag true while button is held past 500ms
    if (_octMinus_longPressTriggered) {
      _events.octMinus_isLongPressed = true;
    }
    if (_events.octMinus_isLongPressed && _events.live_encoderTurned) {
      _events.combo_OctMinus_LiveMoved = true;
      _octMinus_comboHappened = true;
    }
  } else if (_btnOctMinus.wasReleased()) {
    if (!_octMinus_comboHappened && !_octMinus_longPressTriggered) {
      // Short press without combo and without long press
      _events.octMinus_wasReleasedAsShort = true;
    }
    _octMinus_comboHappened = false;
    _octMinus_longPressTriggered = false;
  }
}