#include "EngineMode1.h"
#include <Arduino.h>
#include <math.h>
#include <limits.h>  // For ULONG_MAX


EngineMode1::EngineMode1() {
  _octaveOffset = 0;
  _latchEnabled = false;
  _noteStackPointer = -1;
  _currentPitchVoltage = PITCH_CV_CENTER_VOLTAGE;
  _targetPitchVoltage = PITCH_CV_CENTER_VOLTAGE;
  _lastActivePitchVoltage = PITCH_CV_CENTER_VOLTAGE;
  _currentAuxVoltage = 0.0f;
  _targetAuxVoltage = 0.0f;
  _auxSmoothingAlpha = AUX_VOLTAGE_SMOOTHING_ALPHA_DEFAULT;
  _glideTime_ms = 0.0f;
  _gateOpen = false;
  _retriggerEvent = false;
  _lastUpdateTime_micros = 0;
  _uiEffectRequested = UIEffect::NONE;
  _livePotDisplayValue = 0;
  _aftertouchDeadzoneOffset = 0;
  _shiftModeActive = false;

  for (int i = 0; i < NOTE_STACK_SIZE; ++i) {
    _noteStack[i] = {0, 0};
  }
}

void EngineMode1::begin() {
  _lastUpdateTime_micros = micros();
  updateNotePriority();
}

void EngineMode1::update() {
  unsigned long now = micros();
  
  // Handle microsecond overflow (wraps every ~70 minutes)
  unsigned long deltaTime_micros;
  if (now >= _lastUpdateTime_micros) {
    deltaTime_micros = now - _lastUpdateTime_micros;
  } else {
    // Overflow occurred, calculate remaining time to max + time from 0 to now
    deltaTime_micros = (ULONG_MAX - _lastUpdateTime_micros) + now + 1;
  }
  
  float deltaTime_ms = deltaTime_micros / 1000.0f;
  _lastUpdateTime_micros = now;

  if (_glideTime_ms > 5.0f) {
    float alpha = 1.0f - expf(-deltaTime_ms / _glideTime_ms);
    _currentPitchVoltage = (1.0f - alpha) * _currentPitchVoltage + alpha * _targetPitchVoltage;
  } else {
    _currentPitchVoltage = _targetPitchVoltage;
  }
  
  _currentAuxVoltage = (1.0f - _auxSmoothingAlpha) * _currentAuxVoltage + _auxSmoothingAlpha * _targetAuxVoltage;
}

void EngineMode1::processInputs(const InputEvents& events, const bool* physicalKeyState) {
  // Shift mode detection
  bool shiftPlus = events.octPlus_isLongPressed;
  bool shiftMinus = events.octMinus_isLongPressed;

  // --- Button Events ---
  if (events.hold_wasPressedShort) {
    setLatch(!_latchEnabled, physicalKeyState);
    _uiEffectRequested = UIEffect::VALIDATE;
  }
  if (events.octPlus_wasReleasedAsShort) {
    if (_octaveOffset < MAX_OCTAVE) _octaveOffset++;
    updateNotePriority();
  }
  if (events.octMinus_wasReleasedAsShort) {
    if (_octaveOffset > MIN_OCTAVE) _octaveOffset--;
    updateNotePriority();
  }

  // --- Encoder Control (Incremental) ---
  if (shiftPlus && events.live_encoderTurned) {
    // Direct smoothing control - no catching needed!
    float step = (AUX_SMOOTHING_MAX_ALPHA - AUX_SMOOTHING_MIN_ALPHA) / 100.0f;
    float newAlpha = _auxSmoothingAlpha + (events.live_encoderDelta * step);
    setAuxSmoothingAlpha(constrain(newAlpha, AUX_SMOOTHING_MIN_ALPHA, AUX_SMOOTHING_MAX_ALPHA));
  } 
  else if (shiftMinus && events.live_encoderTurned) {
    // Direct deadzone control - no catching needed!
    int step = AFTERTOUCH_DEADZONE_MAX_OFFSET / 50; // 50 steps from 0 to max
    _aftertouchDeadzoneOffset = constrain(
      _aftertouchDeadzoneOffset + (events.live_encoderDelta * step),
      0, AFTERTOUCH_DEADZONE_MAX_OFFSET
    );
  }
  else if (events.live_encoderTurned) {
    // Glide control with velocity-based smooth acceleration
    float stepSize = calculateEncoderStep(
      events.live_encoderVelocity,
      GLIDE_STEP_MIN,
      GLIDE_STEP_MAX,
      GLIDE_ACCEL_CURVE
    );
    
    _glideTime_ms = constrain(
      _glideTime_ms + (events.live_encoderDelta * stepSize),
      0.0f, GLIDE_MAX_TIME_MS
    );
    _livePotDisplayValue = (_glideTime_ms / GLIDE_MAX_TIME_MS) * 100;
  }
}

void EngineMode1::setAuxSmoothingAlpha(float alpha) {
  _auxSmoothingAlpha = constrain(alpha, AUX_SMOOTHING_MIN_ALPHA, AUX_SMOOTHING_MAX_ALPHA);
}

void EngineMode1::onNoteOn(uint8_t pitch, uint16_t value) {
  _gateOpen = true;
  _retriggerEvent = true;
  pushNote(pitch, value);
  updateNotePriority();
}

void EngineMode1::onNoteOff(uint8_t pitch) {
  if (!_latchEnabled) {
    popNote(pitch);
    updateNotePriority();
  }
}

void EngineMode1::onAftertouchUpdate(uint8_t keyIndex, uint16_t pressure) {
  uint8_t targetPitch = 36 + keyIndex;
  
  // Only process if we have notes in the stack
  if (_noteStackPointer < 0) return;
  
  // Check if this is the active note first (most common case)
  if (_noteStack[_noteStackPointer].pitch == targetPitch) {
    _noteStack[_noteStackPointer].value = pressure;
    updateNotePriority();  // Update output immediately for active note
    return;
  }
  
  // If not active note, just update the pressure value in stack
  // No need to call updateNotePriority() since it won't affect output
  for (int i = 0; i < _noteStackPointer; i++) {  // Note: < instead of <= since we already checked top
    if (_noteStack[i].pitch == targetPitch) {
      _noteStack[i].value = pressure;
      break;  // Only one instance per pitch after popNote in pushNote
    }
  }
}


float EngineMode1::getPitchVoltage() const { return _currentPitchVoltage; }
float EngineMode1::getAuxVoltage() const { return _currentAuxVoltage; }
bool EngineMode1::getGateState() const { return _gateOpen; }
int EngineMode1::getOctaveOffset() const { return _octaveOffset; }
bool EngineMode1::isLatchActive() const { return _latchEnabled; }
int EngineMode1::getLivePotDisplayValue() const { return _livePotDisplayValue; }
int EngineMode1::getAftertouchDeadzoneOffset() const { return _aftertouchDeadzoneOffset; }
float EngineMode1::getAuxSmoothingAlpha() const { return _auxSmoothingAlpha; }
bool EngineMode1::getAndClearRetriggerEvent() { bool e = _retriggerEvent; _retriggerEvent = false; return e; }
UIEffect EngineMode1::getAndClearRequestedEffect() { UIEffect e = _uiEffectRequested; _uiEffectRequested = UIEffect::NONE; return e; }

void EngineMode1::setLatch(bool enabled, const bool* physicalKeyState) {
  _latchEnabled = enabled;
  if (!_latchEnabled && physicalKeyState != nullptr) {
    Note newStack[NOTE_STACK_SIZE];
    int newStackPointer = -1;
    for (int i = 0; i <= _noteStackPointer; i++) {
      Note currentNote = _noteStack[i];
      int keyIndex = currentNote.pitch - 36;
      if (keyIndex >= 0 && keyIndex < NUM_KEYS && physicalKeyState[keyIndex]) {
        if (newStackPointer < NOTE_STACK_SIZE - 1) {
          newStackPointer++;
          newStack[newStackPointer] = currentNote;
        }
      }
    }
    memcpy(_noteStack, newStack, sizeof(_noteStack));
    _noteStackPointer = newStackPointer;
    updateNotePriority();
  }
}

void EngineMode1::pushNote(uint8_t pitch, uint16_t value) {
  popNote(pitch);
  if (_noteStackPointer >= NOTE_STACK_SIZE - 1) {
    for (int i = 0; i < _noteStackPointer; ++i) _noteStack[i] = _noteStack[i + 1];
    _noteStackPointer--;
  }
  if (_noteStackPointer < NOTE_STACK_SIZE - 1) {
    _noteStackPointer++;
    _noteStack[_noteStackPointer] = {pitch, value};
  }
}

void EngineMode1::popNote(uint8_t pitch) {
  for (int i = _noteStackPointer; i >= 0; --i) {
    if (_noteStack[i].pitch == pitch) {
      for (int j = i; j < _noteStackPointer; ++j) _noteStack[j] = _noteStack[j + 1];
      _noteStackPointer--;
      break;
    }
  }
}

void EngineMode1::updateNotePriority() {
  #if DEBUG_LEVEL == 1
    static uint8_t lastLoggedPitch = 0;
  #endif
  
  if (_noteStackPointer < 0) {
    if (_gateOpen) {
      #if DEBUG_LEVEL == 1
        Serial.print("[MUSICAL] : Note "); Serial.print(lastLoggedPitch); Serial.println(" : OFF");
      #endif
    }
    _gateOpen = false;
    _targetAuxVoltage = 0.0f;
    _targetPitchVoltage = _lastActivePitchVoltage;
    return;
  }
  Note activeNote = _noteStack[_noteStackPointer];
  static uint8_t lastActivePitch = 0;
  if(activeNote.pitch != lastActivePitch && _gateOpen) {
    _retriggerEvent = true;
  }
  
  #if DEBUG_LEVEL == 1
    if (_retriggerEvent) {
      lastLoggedPitch = activeNote.pitch;
      Serial.print("[MUSICAL] : Note "); Serial.print(activeNote.pitch); Serial.println(" : ON");
    }
  #endif
  lastActivePitch = activeNote.pitch;

  _targetPitchVoltage = midiNoteToVoltage(activeNote.pitch);
  _lastActivePitchVoltage = _targetPitchVoltage;
  _targetAuxVoltage = ((float)activeNote.value / CV_OUTPUT_RESOLUTION) * DAC_OUTPUT_VOLTAGE_RANGE;

  #if DEBUG_LEVEL == 1
    static float lastLoggedPitchV = -1.0f;
    static float lastLoggedAuxV = -1.0f;
    const float VOLTAGE_LOG_THRESHOLD = 0.02f;

    if (_gateOpen && (abs(_targetPitchVoltage - lastLoggedPitchV) > VOLTAGE_LOG_THRESHOLD || abs(_currentAuxVoltage - lastLoggedAuxV) > VOLTAGE_LOG_THRESHOLD)) {
        Serial.print("[MUSICAL] : Note "); Serial.print(activeNote.pitch);
        Serial.print(" : 1v/o: "); Serial.print(_targetPitchVoltage, 2);
        Serial.print("V ------- AUX:"); Serial.print(_currentAuxVoltage, 2); Serial.println("V");
        
        lastLoggedPitchV = _targetPitchVoltage;
        lastLoggedAuxV = _currentAuxVoltage;
    }
  #endif
}

float EngineMode1::midiNoteToVoltage(uint8_t note) const {
  int finalNoteWithOctave = note + (_octaveOffset * 12);
  float noteDelta = finalNoteWithOctave - PITCH_REFERENCE_MIDI_NOTE;
  float voltageOffset = (noteDelta / 12.0f) * PITCH_STANDARD_VOLTS_PER_OCTAVE;
  return PITCH_CV_CENTER_VOLTAGE + voltageOffset;
}