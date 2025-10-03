#ifndef ENGINE_MODE_3_H
#define ENGINE_MODE_3_H

#include <stdint.h>
#include "HardwareConfig.h"
#include "KeyboardData.h"
#include "InputManager.h" 

class EngineMode3 {
public:
  EngineMode3() {}

  // --- API Principale (Interface Vide) ---
  void begin() {}
  void update() {}
  // CORRECTION: La signature correspond maintenant à celle de EngineMode1
  void processInputs(const InputEvents& events, const bool* physicalKeyState) {}

  void onMidiNoteOn(uint8_t pitch, uint8_t velocity) {}
  void onMidiNoteOff(uint8_t pitch) {}

  // --- GETTERS (Contrat d'Interface avec valeurs par défaut "sûres") ---
  float getPitchVoltage() const { return PITCH_CV_CENTER_VOLTAGE; }
  float getAuxVoltage() const { return 0.0f; }
  bool  getGateState() const { return false; }
  bool  getAndClearRetriggerEvent() { return false; }
  int   getOctaveOffset() const { return 0; }
  bool  isLatchActive() const { return false; }
  int getLivePotDisplayValue() const { return 0; }
  UIEffect getAndClearRequestedEffect() { return UIEffect::NONE; }
};

#endif // ENGINE_MODE_3_H