#include "EngineMode2.h"
#include <Arduino.h>
#include <stdlib.h>  // For rand()

EngineMode2::EngineMode2() {
  _arpCount = 0;
  _arpIndex = 0;
  _currentPattern = ArpPattern::UP;
  _patternDirection = true;
  _octaveToggle = false;
  _cascadeCount = 0;
  _pedalIndex = 0;
  
  _lastStepTime = 0;
  _lastGateOffTime = 0;
  _bpm = 120;
  _gateLength = 0.5f;
  _gateIsOn = false;
  
  _tempoLedOnTime = 0;
  _tempoLedActive = false;
  
  _currentPitchVoltage = PITCH_CV_CENTER_VOLTAGE;
  _targetPitchVoltage = PITCH_CV_CENTER_VOLTAGE;
  _currentAuxVoltage = 0.0f;
  _gateOpen = false;
  _retriggerEvent = false;
  
  _octaveOffset = 0;
  _latchEnabled = false;
  // Calculate initial bargraph display value from default BPM
  _livePotDisplayValue = map(_bpm, 5, 300, 0, 100);  // Should be ~39 for BPM=120
  _uiEffectRequested = UIEffect::NONE;
  
  for (int i = 0; i < MAX_ARP_NOTES; i++) {
    _arpNotes[i] = 0;
    _arpPressures[i] = 0;
  }
}

void EngineMode2::begin() {
  _lastStepTime = millis();
  randomSeed(analogRead(0));  // Seed random for RANDOM pattern
}

void EngineMode2::update() {
  unsigned long now = millis();
  
  // Handle tempo LED pulse (turn off after short duration)
  if (_tempoLedActive && (now - _tempoLedOnTime >= 10)) {  // 10ms pulse
    // Turn off the tempo LED by restoring normal state
    // We'll let the LED manager handle it on next update
    _tempoLedActive = false;
    // Note: We don't directly turn off the LED here to avoid conflicts
  }
  
  // If only one note or no notes, behave like monophonic mode
  if (_arpCount <= 1) {
    // Make sure tempo LED is off when not arpeggiating
    if (_tempoLedActive) {
      analogWrite(PIN_LED_c, 0);  // Turn off tempo LED
      _tempoLedActive = false;
    }
    
    if (_arpCount == 0) {
      _gateOpen = false;
      _currentAuxVoltage = 0.0f;
    } else {
      // Single note: maintain gate state (don't force always on)
      // Gate was set by onNoteOn and will stay on until removed
      _targetPitchVoltage = midiNoteToVoltage(_arpNotes[0]);
      _currentAuxVoltage = ((float)_arpPressures[0] / CV_OUTPUT_RESOLUTION) * DAC_OUTPUT_VOLTAGE_RANGE;
    }
    _currentPitchVoltage = _targetPitchVoltage;
    return;
  }
  
  // Arpeggiator mode for multiple notes
  float stepTime_ms = 60000.0f / _bpm;
  
  // Check if it's time for next step (maintain timing grid)
  if (now - _lastStepTime >= stepTime_ms) {
    _lastStepTime += stepTime_ms;  // Maintain precise timing grid
    // Handle overflow or large drift case
    if (_lastStepTime < now - stepTime_ms * 2) {
      _lastStepTime = now;  // Resync if too far behind
    }
    stepToNext();
    _retriggerEvent = true;
    _gateIsOn = true;
    _lastGateOffTime = _lastStepTime + (stepTime_ms * _gateLength);
    
    // Trigger tempo LED pulse on beat (direct hardware control)
    // Pulse on every beat for clear tempo indication
    // We use the center LED as it's most visible
    analogWrite(PIN_LED_c, 255);  // Full brightness pulse
    _tempoLedOnTime = now;
    _tempoLedActive = true;
  }
  
  // Update gate state based on gate length
  updateGateState(now);
  
  // Update average pressure from all held notes
  updateAveragePressure();
  
  // Smooth pitch transition (instant for arpeggiator)
  _currentPitchVoltage = _targetPitchVoltage;
  
  // Turn off tempo LED after pulse duration
  if (_tempoLedActive && (now - _tempoLedOnTime >= 10)) {
    analogWrite(PIN_LED_c, 0);  // Turn off
    _tempoLedActive = false;
  }
}

void EngineMode2::processInputs(const InputEvents& events, const bool* physicalKeyState) {
  // Hold button - toggle latch
  if (events.hold_wasPressedShort) {
    bool wasLatched = _latchEnabled;
    setLatch(!_latchEnabled, physicalKeyState);
    // Only reset pattern when turning off latch (switching from latch to normal)
    if (wasLatched && !_latchEnabled) {
      resetPattern();
    }
    _uiEffectRequested = UIEffect::VALIDATE;
  }
  
  // Octave transpose
  if (events.octPlus_wasReleasedAsShort) {
    if (_octaveOffset < MAX_OCTAVE) _octaveOffset++;
  }
  if (events.octMinus_wasReleasedAsShort) {
    if (_octaveOffset > MIN_OCTAVE) _octaveOffset--;
  }
  
  // Encoder controls
  if (events.combo_OctPlus_LiveMoved) {
    // Pattern selection - improved modulo arithmetic
    int pattern = (int)_currentPattern + events.live_encoderDelta;
    int maxPatterns = (int)ArpPattern::MAX_PATTERNS;
    // Proper modulo that handles negative numbers
    pattern = ((pattern % maxPatterns) + maxPatterns) % maxPatterns;
    
    // Keep current position when changing patterns for better UX
    // Only reset direction for UP_DOWN pattern
    ArpPattern oldPattern = _currentPattern;
    _currentPattern = (ArpPattern)pattern;
    
    // Reset direction for UP_DOWN, but keep position
    if (_currentPattern == ArpPattern::UP_DOWN) {
      _patternDirection = true;  // Start going up
    }
    
    // Ensure index is still valid for new pattern
    if (_arpIndex >= _arpCount && _arpCount > 0) {
      _arpIndex = _arpCount - 1;
    }
  }
  else if (events.combo_OctMinus_LiveMoved) {
    // Gate length control - 5% steps for better UX
    float step = 0.05f;  // Increased from 0.01f for faster adjustment
    _gateLength = constrain(_gateLength + (events.live_encoderDelta * step), 0.1f, 0.9f);
  }
  else if (events.live_encoderTurned) {
    // BPM control with acceleration for better UX
    int delta = events.live_encoderDelta;
    int stepSize = 3;  // Base step size (reduced for wider range)
    
    // Acceleration: larger steps for faster turning
    if (abs(delta) > 2) stepSize = 8;
    if (abs(delta) > 4) stepSize = 15;
    
    _bpm = constrain(_bpm + (delta * stepSize), 5, 300);
    _livePotDisplayValue = map(_bpm, 5, 300, 0, 100);
  }
}

void EngineMode2::onNoteOn(uint8_t pitch, uint16_t value) {
  // Don't add if already present
  for (uint8_t i = 0; i < _arpCount; i++) {
    if (_arpNotes[i] == pitch) {
      _arpPressures[i] = value;  // Update pressure
      return;
    }
  }
  
  // Add new note if space available
  if (_arpCount < MAX_ARP_NOTES) {
    // Remember the note that was playing before sorting
    uint8_t previousPlayingNote = (_arpCount > 0 && _arpIndex < _arpCount) ? 
                                   _arpNotes[_arpIndex] : 0;
    
    _arpNotes[_arpCount] = pitch;
    _arpPressures[_arpCount] = value;
    _arpCount++;
    
    // Sort notes for ordered patterns
    sortArpNotes();
    
    // Restore index to the same note if possible after sorting
    if (_arpCount > 1 && previousPlayingNote > 0) {
      for (uint8_t i = 0; i < _arpCount; i++) {
        if (_arpNotes[i] == previousPlayingNote) {
          _arpIndex = i;
          break;
        }
      }
    }
    
    // Reset pattern and trigger if this is the first note
    if (_arpCount == 1) {
      resetPattern();
      _gateOpen = true;
      _retriggerEvent = true;
    }
  } else {
    // Reached max notes - cycle out the oldest (FIFO)
    // This provides better UX than silent failure
    // Remember current playing note
    uint8_t previousPlayingNote = (_arpIndex < _arpCount) ? _arpNotes[_arpIndex] : 0;
    
    // Shift all notes left, removing the oldest
    for (uint8_t i = 0; i < MAX_ARP_NOTES - 1; i++) {
      _arpNotes[i] = _arpNotes[i + 1];
      _arpPressures[i] = _arpPressures[i + 1];
    }
    // Add new note at the end
    _arpNotes[MAX_ARP_NOTES - 1] = pitch;
    _arpPressures[MAX_ARP_NOTES - 1] = value;
    
    // Maintain sort order
    sortArpNotes();
    
    // Try to maintain position on the same note if it still exists
    bool foundNote = false;
    for (uint8_t i = 0; i < _arpCount; i++) {
      if (_arpNotes[i] == previousPlayingNote) {
        _arpIndex = i;
        foundNote = true;
        break;
      }
    }
    
    // If the playing note was removed, adjust index
    if (!foundNote && _arpIndex > 0) {
      _arpIndex--;  // Move to previous position
    }
  }
}

void EngineMode2::onNoteOff(uint8_t pitch) {
  if (!_latchEnabled) {
    removeNote(pitch);
  }
}

void EngineMode2::onAftertouchUpdate(uint8_t keyIndex, uint16_t pressure) {
  uint8_t targetPitch = 36 + keyIndex;
  
  // Update pressure for the note if it's in our array
  for (uint8_t i = 0; i < _arpCount; i++) {
    if (_arpNotes[i] == targetPitch) {
      _arpPressures[i] = pressure;
      break;
    }
  }
}

float EngineMode2::getPitchVoltage() const {
  return _currentPitchVoltage;
}

float EngineMode2::getAuxVoltage() const {
  return _currentAuxVoltage;
}

bool EngineMode2::getGateState() const {
  return _gateOpen;
}

bool EngineMode2::getAndClearRetriggerEvent() {
  bool event = _retriggerEvent;
  _retriggerEvent = false;
  return event;
}

int EngineMode2::getOctaveOffset() const {
  return _octaveOffset;
}

bool EngineMode2::isLatchActive() const {
  return _latchEnabled;
}

int EngineMode2::getLivePotDisplayValue() const {
  return _livePotDisplayValue;
}

UIEffect EngineMode2::getAndClearRequestedEffect() {
  UIEffect effect = _uiEffectRequested;
  _uiEffectRequested = UIEffect::NONE;
  return effect;
}

int EngineMode2::getCurrentPattern() const {
  return (int)_currentPattern;
}

int EngineMode2::getMaxPatterns() const {
  return (int)ArpPattern::MAX_PATTERNS;
}

float EngineMode2::getGateLength() const {
  return _gateLength;
}

// Private helper methods

void EngineMode2::resetPattern() {
  _arpIndex = 0;
  _patternDirection = true;
  _octaveToggle = false;
  _cascadeCount = 0;
  _pedalIndex = 0;
  _lastStepTime = millis();
}

void EngineMode2::stepToNext() {
  if (_arpCount == 0) return;
  
  switch (_currentPattern) {
    case ArpPattern::UP:
      stepPatternUp();
      break;
    case ArpPattern::DOWN:
      stepPatternDown();
      break;
    case ArpPattern::UP_DOWN:
      stepPatternUpDown();
      break;
    case ArpPattern::RANDOM:
      stepPatternRandom();
      break;
    case ArpPattern::CHORD:
      stepPatternChord();
      break;
    case ArpPattern::UP_OCTAVE:
      stepPatternUpOctave();
      break;
    case ArpPattern::DOWN_OCTAVE:
      stepPatternDownOctave();
      break;
    case ArpPattern::CONVERGE:
      stepPatternConverge();
      break;
    case ArpPattern::DIVERGE:
      stepPatternDiverge();
      break;
    case ArpPattern::PEDAL_UP:
      stepPatternPedalUp();
      break;
    case ArpPattern::CASCADE:
      stepPatternCascade();
      break;
    case ArpPattern::PROBABILITY:
      stepPatternProbability();
      break;
    default:
      stepPatternUp();
      break;
  }
  
  updatePitchFromCurrentNote();
}

void EngineMode2::stepPatternUp() {
  _arpIndex++;
  if (_arpIndex >= _arpCount) {
    _arpIndex = 0;
  }
}

void EngineMode2::stepPatternDown() {
  _arpIndex--;
  if (_arpIndex < 0) {
    _arpIndex = _arpCount - 1;
  }
}

void EngineMode2::stepPatternUpDown() {
  // Handle edge cases for 1 or 2 notes
  if (_arpCount <= 1) {
    _arpIndex = 0;
    return;
  }
  
  if (_arpCount == 2) {
    // Simple toggle for 2 notes
    _arpIndex = (_arpIndex == 0) ? 1 : 0;
    return;
  }
  
  // Normal UP_DOWN for 3+ notes
  if (_patternDirection) {
    // Going up
    _arpIndex++;
    if (_arpIndex >= _arpCount - 1) {
      _arpIndex = _arpCount - 1;
      _patternDirection = false;
    }
  } else {
    // Going down
    _arpIndex--;
    if (_arpIndex <= 0) {
      _arpIndex = 0;
      _patternDirection = true;
    }
  }
}

void EngineMode2::stepPatternRandom() {
  if (_arpCount > 1) {
    int newIndex = random(_arpCount);
    // Avoid repeating the same note
    while (newIndex == _arpIndex && _arpCount > 1) {
      newIndex = random(_arpCount);
    }
    _arpIndex = newIndex;
  }
}

void EngineMode2::stepPatternChord() {
  // All notes play at once - just cycle through for CV output
  _arpIndex = (_arpIndex + 1) % _arpCount;
}

void EngineMode2::stepPatternUpOctave() {
  _arpIndex++;
  if (_arpIndex >= _arpCount) {
    _arpIndex = 0;
    _octaveToggle = !_octaveToggle;  // Toggle octave
  }
}

void EngineMode2::stepPatternDownOctave() {
  _arpIndex--;
  if (_arpIndex < 0) {
    _arpIndex = _arpCount - 1;
    _octaveToggle = !_octaveToggle;  // Toggle octave
  }
}

void EngineMode2::stepPatternConverge() {
  // Move from outside to center: 0, n-1, 1, n-2, 2, ...
  if (_arpCount <= 1) {
    _arpIndex = 0;
    return;
  }
  
  static int convergeStep = 0;
  if (_arpIndex == 0 && convergeStep == 0) {
    convergeStep = 0;  // Reset on pattern start
  }
  
  if (convergeStep % 2 == 0) {
    _arpIndex = convergeStep / 2;
  } else {
    _arpIndex = _arpCount - 1 - (convergeStep / 2);
  }
  
  convergeStep++;
  if (convergeStep >= _arpCount * 2) {
    convergeStep = 0;
  }
}

void EngineMode2::stepPatternDiverge() {
  // Move from center to outside
  if (_arpCount <= 1) {
    _arpIndex = 0;
    return;
  }
  
  int center = _arpCount / 2;
  static int divergeOffset = 0;
  
  if (_arpIndex == center && divergeOffset == 0) {
    divergeOffset = 0;  // Reset
  }
  
  if (divergeOffset % 2 == 0) {
    _arpIndex = center - (divergeOffset / 2);
  } else {
    _arpIndex = center + ((divergeOffset + 1) / 2);
  }
  
  // Wrap around
  if (_arpIndex < 0 || _arpIndex >= _arpCount) {
    divergeOffset = 0;
    _arpIndex = center;
  } else {
    divergeOffset++;
  }
}

void EngineMode2::stepPatternPedalUp() {
  // Alternate between lowest note and ascending others
  if (_pedalIndex % 2 == 0) {
    _arpIndex = 0;  // Always return to lowest note
  } else {
    _arpIndex = 1 + (_pedalIndex / 2);
    if (_arpIndex >= _arpCount) {
      _pedalIndex = 0;
      _arpIndex = 0;
      return;
    }
  }
  _pedalIndex++;
}

void EngineMode2::stepPatternCascade() {
  // Play each note twice before moving on
  if (_cascadeCount == 0) {
    _cascadeCount = 1;  // Stay on same note
  } else {
    _cascadeCount = 0;
    _arpIndex++;
    if (_arpIndex >= _arpCount) {
      _arpIndex = 0;
    }
  }
}

void EngineMode2::stepPatternProbability() {
  // Weighted random - lower notes have higher probability
  if (_arpCount == 1) {
    _arpIndex = 0;
    return;
  }
  
  // Create weighted distribution (lower notes more likely)
  int totalWeight = 0;
  for (int i = 0; i < _arpCount; i++) {
    totalWeight += (_arpCount - i);  // Higher weight for lower indices
  }
  
  int randomValue = random(totalWeight);
  int accumWeight = 0;
  
  for (int i = 0; i < _arpCount; i++) {
    accumWeight += (_arpCount - i);
    if (randomValue < accumWeight) {
      _arpIndex = i;
      break;
    }
  }
}

void EngineMode2::updatePitchFromCurrentNote() {
  // Add bounds check for safety
  if (_arpCount > 0 && _arpIndex >= 0 && _arpIndex < _arpCount) {
    // Check if we need octave shift for certain patterns
    if ((_currentPattern == ArpPattern::UP_OCTAVE || 
         _currentPattern == ArpPattern::DOWN_OCTAVE) && _octaveToggle) {
      int octaveShift = (_currentPattern == ArpPattern::UP_OCTAVE) ? 1 : -1;
      _targetPitchVoltage = midiNoteToVoltageWithOctave(_arpNotes[_arpIndex], octaveShift);
    } else {
      _targetPitchVoltage = midiNoteToVoltage(_arpNotes[_arpIndex]);
    }
  }
}

void EngineMode2::updateAveragePressure() {
  if (_arpCount == 0) {
    _currentAuxVoltage = 0.0f;
    return;
  }
  
  // Calculate average pressure from all held notes
  uint32_t totalPressure = 0;
  for (uint8_t i = 0; i < _arpCount; i++) {
    totalPressure += _arpPressures[i];
  }
  
  uint16_t avgPressure = totalPressure / _arpCount;
  _currentAuxVoltage = ((float)avgPressure / CV_OUTPUT_RESOLUTION) * DAC_OUTPUT_VOLTAGE_RANGE;
}

void EngineMode2::updateGateState(unsigned long now) {
  if (_arpCount == 0) {
    _gateOpen = false;
    _gateIsOn = false;
    return;
  }
  
  // Handle gate length timing
  if (_gateIsOn && now >= _lastGateOffTime) {
    _gateIsOn = false;
  }
  
  _gateOpen = _gateIsOn;
}

void EngineMode2::sortArpNotes() {
  // Simple bubble sort for small array
  for (uint8_t i = 0; i < _arpCount - 1; i++) {
    for (uint8_t j = 0; j < _arpCount - i - 1; j++) {
      if (_arpNotes[j] > _arpNotes[j + 1]) {
        // Swap notes
        uint8_t tempNote = _arpNotes[j];
        _arpNotes[j] = _arpNotes[j + 1];
        _arpNotes[j + 1] = tempNote;
        
        // Swap pressures
        uint16_t tempPressure = _arpPressures[j];
        _arpPressures[j] = _arpPressures[j + 1];
        _arpPressures[j + 1] = tempPressure;
      }
    }
  }
}

void EngineMode2::setLatch(bool enabled, const bool* physicalKeyState) {
  _latchEnabled = enabled;
  
  if (!_latchEnabled && physicalKeyState != nullptr) {
    // Remove notes that are no longer physically pressed
    uint8_t newNotes[MAX_ARP_NOTES];
    uint16_t newPressures[MAX_ARP_NOTES];
    uint8_t newCount = 0;
    
    for (uint8_t i = 0; i < _arpCount; i++) {
      int keyIndex = _arpNotes[i] - 36;
      if (keyIndex >= 0 && keyIndex < NUM_KEYS && physicalKeyState[keyIndex]) {
        newNotes[newCount] = _arpNotes[i];
        newPressures[newCount] = _arpPressures[i];
        newCount++;
      }
    }
    
    // Update arrays
    for (uint8_t i = 0; i < newCount; i++) {
      _arpNotes[i] = newNotes[i];
      _arpPressures[i] = newPressures[i];
    }
    _arpCount = newCount;
    
    // Reset index if needed
    if (_arpIndex >= _arpCount && _arpCount > 0) {
      _arpIndex = _arpCount - 1;
    }
  }
}

float EngineMode2::midiNoteToVoltage(uint8_t note) const {
  int finalNoteWithOctave = note + (_octaveOffset * 12);
  float noteDelta = finalNoteWithOctave - PITCH_REFERENCE_MIDI_NOTE;
  float voltageOffset = (noteDelta / 12.0f) * PITCH_STANDARD_VOLTS_PER_OCTAVE;
  return PITCH_CV_CENTER_VOLTAGE + voltageOffset;
}

float EngineMode2::midiNoteToVoltageWithOctave(uint8_t note, int additionalOctave) const {
  int finalNoteWithOctave = note + ((_octaveOffset + additionalOctave) * 12);
  float noteDelta = finalNoteWithOctave - PITCH_REFERENCE_MIDI_NOTE;
  float voltageOffset = (noteDelta / 12.0f) * PITCH_STANDARD_VOLTS_PER_OCTAVE;
  return PITCH_CV_CENTER_VOLTAGE + voltageOffset;
}

void EngineMode2::removeNote(uint8_t pitch) {
  // Find and remove the note
  int removeIndex = -1;
  for (uint8_t i = 0; i < _arpCount; i++) {
    if (_arpNotes[i] == pitch) {
      removeIndex = i;
      break;
    }
  }
  
  if (removeIndex >= 0) {
    // Remember if we need to adjust the playing index
    bool adjustIndex = (removeIndex < _arpIndex);
    
    // Shift remaining notes down
    for (uint8_t i = removeIndex; i < _arpCount - 1; i++) {
      _arpNotes[i] = _arpNotes[i + 1];
      _arpPressures[i] = _arpPressures[i + 1];
    }
    _arpCount--;
    
    // Adjust index intelligently
    if (adjustIndex && _arpIndex > 0) {
      _arpIndex--;  // Compensate for removed note before current position
    } else if (_arpIndex >= _arpCount && _arpCount > 0) {
      _arpIndex = _arpCount - 1;
    } else if (_arpCount == 0) {
      _arpIndex = 0;
      _gateOpen = false;
    }
  }
}