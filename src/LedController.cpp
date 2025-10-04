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
          // Non-linear mapping: expand low values (0.001-0.225) to 0-50%
          // and compress high values (0.225-0.9) to 50-100%
          float currentAlpha = engine1.getAuxSmoothingAlpha();
          float normalizedAlpha = (currentAlpha - AUX_SMOOTHING_MIN_ALPHA) / 
                                  (AUX_SMOOTHING_MAX_ALPHA - AUX_SMOOTHING_MIN_ALPHA);
          
          // Apply exponential curve to expand low range
          // Use power of 0.4 to heavily favor the low end
          float remappedValue = powf(normalizedAlpha, 0.4f);
          displayValue = (int)(remappedValue * 100.0f);
          displayValue = constrain(displayValue, 0, 100);
          
          _ledManager.displayInvertedBargraph(displayValue);
          break;
        }
        case MODE_INTERVAL: {
          // Show current pattern continuously during long press
          _ledManager.playPatternDisplay(engine2.getCurrentPattern());
          break;
        }
        case MODE_MIDI:
          // Mode 3 doesn't use Oct+ combo yet
          displayValue = 0;
          _ledManager.displayInvertedBargraph(displayValue);
          break;
      }
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
          // Show shuffle: template + depth as continuous inverted bargraph
          // Each template occupies 20% of display (5 templates × 20% = 100%)
          // Within each template, depth goes from 0-50% → mapped to 0-10% of display
          uint8_t templateIndex = engine2.getTemplate();
          float shuffleDepth = engine2.getShuffleDepth();
          
          // Calculate display value:
          // Template 0: 0-10%, Template 1: 20-30%, etc.
          float baseValue = templateIndex * 20.0f;
          float depthContribution = (shuffleDepth / SHUFFLE_DEPTH_MAX) * 10.0f;
          displayValue = (int)(baseValue + depthContribution);
          displayValue = constrain(displayValue, 0, 100);
          
          _ledManager.displayInvertedBargraph(displayValue);
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
  } else if (requestedEffect == UIEffect::ARP_PATTERN_CHANGE) {
    _ledManager.playPatternDisplay(engine2.getCurrentPattern());
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