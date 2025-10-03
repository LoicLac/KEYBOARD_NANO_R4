#include "LedController.h"

LedController::LedController() {
  _lastDisplayedOctave = -99;
  _lastDisplayedMode = (GameMode)-1;
}

void LedController::begin() {
  _ledManager.begin();
}

// CORRECTION: La signature de la fonction correspond maintenant à celle du .h
void LedController::update(GameMode mode, const InputEvents& events,
                           EngineMode1& engine1, EngineMode2& engine2, EngineMode3& engine3,
                           CapacitiveKeyboard& keyboard) 
{
  // --- ÉTAPE 1: GESTION DES ÉVÉNEMENTS PRIORITAIRES (CHANGEMENT DE MODE) ---
  if (mode != _lastDisplayedMode) {
    switch(mode) {
      case MODE_PRESSURE_GLIDE: _ledManager.playChase(30, 2); break;
      case MODE_INTERVAL:       _ledManager.playCrossfade(200, 3); break;
      case MODE_MIDI:           _ledManager.playInwardWipe(80, 2); break;
    }
  }

  // --- ÉTAPE 2: GESTION DES EFFETS D'AVANT-PLAN (INPUTS) ---
  bool isInShiftMode = events.octPlus_isLongPressed || events.octMinus_isLongPressed;

  if (isInShiftMode) {
    if (events.octPlus_isLongPressed) {
      int displayValue = 0;
      switch(mode) {
        case MODE_PRESSURE_GLIDE: {
          // Show current smoothing value as inverted bargraph
          float currentAlpha = engine1.getAuxSmoothingAlpha();
          displayValue = map(currentAlpha * 1000, 
                            AUX_SMOOTHING_MIN_ALPHA * 1000, 
                            AUX_SMOOTHING_MAX_ALPHA * 1000, 
                            0, 100);
          break;
        }
        case MODE_INTERVAL: {
          // Show current arpeggiator pattern as inverted bargraph (scalable)
          int currentPattern = engine2.getCurrentPattern();
          int maxPatterns = engine2.getMaxPatterns();
          // Map pattern index to 0-100% for display
          displayValue = (maxPatterns > 1) ? 
                        map(currentPattern, 0, maxPatterns - 1, 0, 100) : 0;
          break;
        }
        case MODE_MIDI:
          // Mode 3 doesn't use Oct+ combo yet
          displayValue = 0;
          break;
      }
      _ledManager.displayInvertedBargraph(displayValue);
    } 
    else if (events.octMinus_isLongPressed) {
      int displayValue = 0;
      switch(mode) {
        case MODE_PRESSURE_GLIDE: {
          // Show current deadzone value as inverted bargraph  
          displayValue = map(engine1.getAftertouchDeadzoneOffset(), 
                            0, AFTERTOUCH_DEADZONE_MAX_OFFSET, 
                            0, 100);
          break;
        }
        case MODE_INTERVAL: {
          // Show current gate length as inverted bargraph (10%-90%)
          float gateLength = engine2.getGateLength();
          displayValue = map(gateLength * 100, 10, 90, 0, 100);
          break;
        }
        case MODE_MIDI:
          // Mode 3 doesn't use Oct- combo yet
          displayValue = 0;
          break;
      }
      _ledManager.displayInvertedBargraph(displayValue);
    }
  } 
  // Sinon, on gère les bargraphs normaux
  else {
    if (events.sens_potMoved) {
      _ledManager.displayBargraph(map(events.potSensValue, 0, 1023, 0, 100));
    }
    if (events.live_encoderTurned) {
      // Show current value based on mode: glide (mode1) or BPM (mode2)
      int displayValue = 0;
      switch(mode) {
        case MODE_PRESSURE_GLIDE: 
          displayValue = engine1.getLivePotDisplayValue(); 
          break;
        case MODE_INTERVAL:       
          displayValue = engine2.getLivePotDisplayValue(); 
          break;
        case MODE_MIDI:           
          displayValue = engine3.getLivePotDisplayValue(); 
          break;
      }
      _ledManager.displayBargraph(displayValue);
    }
  }

  UIEffect requestedEffect = UIEffect::NONE;
  switch(mode) {
    case MODE_PRESSURE_GLIDE: requestedEffect = engine1.getAndClearRequestedEffect(); break;
    case MODE_INTERVAL:       requestedEffect = engine2.getAndClearRequestedEffect(); break;
    case MODE_MIDI:           requestedEffect = engine3.getAndClearRequestedEffect(); break;
  }
  if (requestedEffect == UIEffect::VALIDATE) {
    _ledManager.playValidation(100, 2);
  }

  // --- ÉTAPE 3: GESTION DE L'AFFICHAGE DE FOND ---
  int currentOctave = 0;
  switch(mode) {
    case MODE_PRESSURE_GLIDE: currentOctave = engine1.getOctaveOffset(); break;
    case MODE_INTERVAL:       currentOctave = engine2.getOctaveOffset(); break;
    case MODE_MIDI:           currentOctave = engine3.getOctaveOffset(); break;
  }

  if (mode != _lastDisplayedMode || currentOctave != _lastDisplayedOctave) {
    _ledManager.displayOctave(currentOctave, LED_OCTAVE_BREATHE_PERIOD_MS);
    _lastDisplayedOctave = currentOctave;
  }

  _lastDisplayedMode = mode;

  // --- ÉTAPE 4: MISE À JOUR FINALE DU MOTEUR D'ANIMATION ---
  _ledManager.update();
}