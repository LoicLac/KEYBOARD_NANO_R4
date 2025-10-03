#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include "LedManager.h"
#include "InputManager.h"
#include "EngineMode1.h"
#include "EngineMode2.h"
#include "EngineMode3.h"
#include "HardwareConfig.h"
#include "CapacitiveKeyboard.h"

// =================================================================
// Classe LedController (Le "Cerveau" de l'Affichage)
// =================================================================
class LedController {
public:
  LedController();

  void begin();

  void update(GameMode mode, const InputEvents& events, 
              EngineMode1& engine1, EngineMode2& engine2, EngineMode3& engine3,
              CapacitiveKeyboard& keyboard);

private:
  LedManager _ledManager; 

  int _lastDisplayedOctave;
  GameMode _lastDisplayedMode;
};

#endif // LED_CONTROLLER_H