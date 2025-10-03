#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "HardwareConfig.h"
#include "SimpleEncoder.h"
#include <JC_Button.h>

struct InputEvents {
  // --- Événements de Boutons ---
  bool hold_wasPressedShort;
  bool hold_wasPressedLong;
  bool mode_wasPressedShort;
  bool mode_wasPressedLong;
  // RESTAURATION: Ce nom est plus clair et correct
  bool octPlus_wasReleasedAsShort;
  bool octMinus_wasReleasedAsShort;
  
  // États des boutons "shift"
  bool octPlus_isPressed;
  bool octMinus_isPressed;

  // --- Mouvements de Contrôleurs ---
  bool sens_potMoved;
  bool live_encoderTurned;     // Replaces live_potMoved
  int potSensValue; 
  int live_encoderDelta;       // Replaces potLiveValue - contains +1, -1, or 0

  // --- Événements de Combinaisons ---
  bool combo_OctPlus_LiveMoved;
  bool combo_OctMinus_LiveMoved;
};

class InputManager {
public:
  InputManager();
  void begin();
  void update();
  const InputEvents& getEvents() const;
  static bool isHoldPressedOnBoot();

private:
  Button _btnHold;
  Button _btnMode;
  Button _btnOctPlus;
  Button _btnOctMinus;
  InputEvents _events; 
  SimpleEncoder _liveEncoder;  // Replaces pot_live
  float _smoothedPotSens;
  int _lastPotSensSent;
  bool _holdLongPressTriggered;
  bool _modeLongPressTriggered;
  bool _octPlus_comboHappened;
  bool _octMinus_comboHappened;
};

#endif // INPUT_MANAGER_H