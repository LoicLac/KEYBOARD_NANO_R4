#ifndef ENGINE_MODE_1_H
#define ENGINE_MODE_1_H

#include <stdint.h>
#include "HardwareConfig.h" 
#include "KeyboardData.h"   
#include "InputManager.h"   

class EngineMode1 {
public:
  enum class ParamID {
      GLIDE,
      SMOOTHING,
      DEADZONE
  };

  EngineMode1();

  // --- API Principale ---
  void begin();
  void update();
  // La signature a besoin de l'état du clavier pour la fonction Latch
  void processInputs(const InputEvents& events, const bool* physicalKeyState);

  // Méthodes pour le scan clavier (appelées par le .ino)
  void onNoteOn(uint8_t pitch, uint16_t value);
  void onNoteOff(uint8_t pitch);
  void onAftertouchUpdate(uint8_t keyIndex, uint16_t pressure);
  
  // --- GETTERS (API de sortie) ---
  float getPitchVoltage() const;
  float getAuxVoltage() const;
  bool  getGateState() const;
  bool  getAndClearRetriggerEvent();
  int   getOctaveOffset() const;
  bool  isLatchActive() const;
  int getLivePotDisplayValue() const;
  int getAftertouchDeadzoneOffset() const;
  float getAuxSmoothingAlpha() const;  // ADD: needed for LED display
  UIEffect getAndClearRequestedEffect();

private:
  // --- Méthodes privées ---
  void pushNote(uint8_t pitch, uint16_t value);
  void popNote(uint8_t pitch);
  void updateNotePriority();
  float midiNoteToVoltage(uint8_t note) const;
  void setLatch(bool enabled, const bool* physicalKeyState);
  void setAuxSmoothingAlpha(float alpha); // Gardée pour la combinaison

  // --- État interne ---
  int _octaveOffset;
  bool _latchEnabled;
  UIEffect _uiEffectRequested;
  int _livePotDisplayValue;
  int _aftertouchDeadzoneOffset;

  Note _noteStack[NOTE_STACK_SIZE];
  int _noteStackPointer;

  float _currentPitchVoltage;
  float _targetPitchVoltage;
  float _glideTime_ms;
  float _lastActivePitchVoltage;

  float _currentAuxVoltage;
  float _targetAuxVoltage;
  float _auxSmoothingAlpha;

  bool _gateOpen;
  bool _retriggerEvent;

  unsigned long _lastUpdateTime_micros;
};

#endif // ENGINE_MODE_1_H