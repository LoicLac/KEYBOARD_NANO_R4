#ifndef ENGINE_MODE_2_H
#define ENGINE_MODE_2_H

#include <stdint.h>
#include "HardwareConfig.h"
#include "KeyboardData.h"
#include "InputManager.h"

// Arpeggiator patterns - easy to extend
enum class ArpPattern {
  UP = 0,
  DOWN,
  UP_DOWN,
  RANDOM,
  CHORD,        // All notes at once
  UP_OCTAVE,    // Ascending then octave jump
  DOWN_OCTAVE,  // Descending then octave drop
  CONVERGE,     // Outside to inside
  DIVERGE,      // Inside to outside
  PEDAL_UP,     // Alternate with lowest note
  CASCADE,      // Each note twice
  PROBABILITY,  // Weighted random (lower notes favored)
  MAX_PATTERNS  // Always last - used for cycling
};

class EngineMode2 {
public:
  EngineMode2();

  // --- Main API ---
  void begin();
  void update();
  void processInputs(const InputEvents& events, const bool* physicalKeyState);

  void onNoteOn(uint8_t pitch, uint16_t value);
  void onNoteOff(uint8_t pitch);
  void onAftertouchUpdate(uint8_t keyIndex, uint16_t pressure);

  // --- Getters ---
  float getPitchVoltage() const;
  float getAuxVoltage() const;
  bool  getGateState() const;
  bool  getAndClearRetriggerEvent();
  int   getOctaveOffset() const;
  bool  isLatchActive() const;
  int   getLivePotDisplayValue() const;
  UIEffect getAndClearRequestedEffect();
  
  // Additional getters for LED display
  int   getCurrentPattern() const;
  int   getMaxPatterns() const;
  float getGateLength() const;
  
  // Setter for shared aftertouch parameters from Engine1
  void setSharedAftertouchParams(float smoothingAlpha);

private:
  // Arpeggiator state
  static const uint8_t MAX_ARP_NOTES = 8;
  uint8_t _arpNotes[MAX_ARP_NOTES];        // Note pitches
  uint16_t _arpPressures[MAX_ARP_NOTES];   // Pressure per note
  unsigned long _lastNotePressTime[MAX_ARP_NOTES];  // For double-tap detection
  uint8_t _arpCount;                       // Number of active notes
  int8_t _arpIndex;                        // Current position in pattern
  
  // Pattern control
  ArpPattern _currentPattern;
  bool _patternDirection;                  // For UP_DOWN pattern
  bool _octaveToggle;                      // For UP_OCTAVE/DOWN_OCTAVE patterns
  uint8_t _cascadeCount;                   // For CASCADE pattern
  uint8_t _pedalIndex;                     // For PEDAL_UP pattern
  
  // Timing
  unsigned long _lastStepTime;
  unsigned long _lastGateOffTime;
  uint16_t _bpm;
  float _gateLength;                       // 0.1 to 0.9
  bool _gateIsOn;
  
  // Output values
  float _currentPitchVoltage;
  float _targetPitchVoltage;
  float _currentAuxVoltage;
  float _targetAuxVoltage;                 // Target for smoothing
  float _auxSmoothingAlpha;                // Shared from Engine1
  bool _gateOpen;
  bool _retriggerEvent;
  
  // Control state
  int _octaveOffset;
  bool _latchEnabled;
  int _livePotDisplayValue;
  UIEffect _uiEffectRequested;
  bool _shiftModeActive;
  
  // Helper methods
  void resetPattern();
  void stepToNext();
  void stepPatternUp();
  void stepPatternDown();
  void stepPatternUpDown();
  void stepPatternRandom();
  void stepPatternChord();
  void stepPatternUpOctave();
  void stepPatternDownOctave();
  void stepPatternConverge();
  void stepPatternDiverge();
  void stepPatternPedalUp();
  void stepPatternCascade();
  void stepPatternProbability();
  void updatePitchFromCurrentNote();
  void updateCurrentNotePressure();
  void updateGateState(unsigned long now);
  void sortArpNotes();
  void setLatch(bool enabled, const bool* physicalKeyState);
  float midiNoteToVoltage(uint8_t note) const;
  float midiNoteToVoltageWithOctave(uint8_t note, int octaveOffset) const;
  void removeNote(uint8_t pitch);
};

#endif // ENGINE_MODE_2_H