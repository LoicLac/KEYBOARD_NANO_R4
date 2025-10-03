#include "KeyboardCalibrator.h"
#include "CapacitiveKeyboard.h"
#include "LedManager.h"
#include "DACManager.h"
#include "KeyboardData.h"
#include "HardwareConfig.h"

#include <Arduino.h>
#include <JC_Button.h>

const uint16_t sensitivityTargets[] = { 550, 650, 750, 850, 900 };
const char* sensitivityNames[] = {"Standard", "Sensible", "Tres Sensible", "Haute Perf.", "Gain Max"};
const int NUM_SENSITIVITY_LEVELS = sizeof(sensitivityTargets) / sizeof(uint16_t);


KeyboardCalibrator::KeyboardCalibrator() {
  // Constructeur vide
}

void KeyboardCalibrator::run(
  CapacitiveKeyboard &keyboard, 
  LedManager &leds,
  DACManager &dac,
  Button &holdBtn, 
  Button &modeBtn, 
  Button &octPlusBtn, 
  Button &octMinusBtn
) {
  
  // Initialise le matériel clavier (I2C)
  keyboard.initializeHardware();
  
  // Met les sorties CV à zéro pour la sécurité pendant toute la calibration
  dac.setOutputVoltage(0, 0.0f);
  dac.setOutputVoltage(1, 0.0f);

  // Sas de sécurité pour purger l'état du bouton initial
  Serial.println("\n[CAL] Mode calibration detecte. Veuillez relacher le bouton HOLD.");
  while(holdBtn.isPressed()) {
    holdBtn.read();
    delay(10);
  }
  holdBtn.read();
  (void)holdBtn.wasPressed();
  
  
  enum CalFsmState { 
    STATE_INIT, 
    STATE_TUNE_SENSITIVITY, 
    STATE_APPLYING_CONFIG, 
    STATE_WAIT_RELEASE_AFTER_APPLY, 
    STATE_WAIT_RELEASE_AFTER_TUNE, 
    STATE_PREPARE_KEY, 
    STATE_MEASURE_KEY, 
    STATE_WAIT_RELEASE_AFTER_MEASURE, 
    STATE_FINAL_CONFIRMATION,
    STATE_SAVE_EXIT, 
    STATE_FINISHED 
  };
  CalFsmState currentState = STATE_INIT;
  
  int currentKey = 0;
  int sensitivityIndex = 0;
  bool helpDisplayed = false;
  bool recapDisplayed = false;
  uint16_t referenceBaselines[NUM_KEYS];
  uint16_t measuredDeltas[NUM_KEYS];
  uint16_t currentMaxDelta = 0, lastPrintedMaxDelta = 0;
  unsigned long lastPrintTime = 0;

  Serial.println("[FSM] Lancement de la machine a etats de calibration...");

  while (currentState != STATE_FINISHED) {
    holdBtn.read(); 
    modeBtn.read(); 
    octPlusBtn.read(); 
    octMinusBtn.read();
    keyboard.pollAllSensorData(); 
    leds.update();

    switch (currentState) {
      case STATE_INIT:
        Serial.println("[FSM] -> ETAT: STATE_INIT");
        leds.enterCalibrationMode();
        Serial.println("\n[CAL] Demarrage de la calibration V5");
        keyboard.runAutoconfiguration(sensitivityTargets[sensitivityIndex]);
        leds.playCountdown(CAL_AUTOCONFIG_COUNTDOWN_MS);
        currentState = STATE_TUNE_SENSITIVITY;
        break;

      case STATE_TUNE_SENSITIVITY:
        if (!helpDisplayed) {
          Serial.println("[FSM] -> ETAT: STATE_TUNE_SENSITIVITY");
          Serial.println("\n=== Phase 1: Reglage de la Sensibilite ===");
          Serial.println("ACTIONS:");
          Serial.println("  - OCT+ / OCT- : Choisir un preset de sensibilite.");
          Serial.println("  - MODE        : Appliquer le preset selectionne et observer la baseline.");
          Serial.println("  - HOLD        : Valider la configuration actuelle et passer a l'etape suivante.");
          leds.displayStaticPattern(1 << sensitivityIndex, false);
          helpDisplayed = true;
        }

        if (millis() - lastPrintTime > 500) {
           Serial.print("\n>>> Selection: ["); Serial.print(sensitivityNames[sensitivityIndex]);
           Serial.print("] (Cible: "); Serial.print(sensitivityTargets[sensitivityIndex]);
           Serial.println(") <<< ACTIONS: [OCT+/–] Changer | [MODE] Appliquer | [HOLD] Valider");
           keyboard.logFullBaselineTable();
           lastPrintTime = millis();
        }

        if (octPlusBtn.wasPressed() && sensitivityIndex < NUM_SENSITIVITY_LEVELS - 1) {
          sensitivityIndex++;
          leds.displayStaticPattern(1 << sensitivityIndex, false);
        }
        if (octMinusBtn.wasPressed() && sensitivityIndex > 0) {
          sensitivityIndex--;
          leds.displayStaticPattern(1 << sensitivityIndex, false);
        }

        if (modeBtn.wasPressed()) {
          currentState = STATE_APPLYING_CONFIG;
        }
        
        if (holdBtn.wasPressed()) {
          currentState = STATE_WAIT_RELEASE_AFTER_TUNE;
        }
        break;

      case STATE_APPLYING_CONFIG:
        Serial.println("[FSM] -> ETAT: STATE_APPLYING_CONFIG");
        {
          uint16_t target = sensitivityTargets[sensitivityIndex];
          Serial.print("\n[TUNE] Application du preset de sensibilite... Cible: "); Serial.println(target);
          leds.playValidation(100, 1);
          keyboard.runAutoconfiguration(target);
          currentState = STATE_WAIT_RELEASE_AFTER_APPLY;
        }
        break;

      case STATE_WAIT_RELEASE_AFTER_APPLY:
        if (modeBtn.wasReleased()) {
          helpDisplayed = false;
          currentState = STATE_TUNE_SENSITIVITY;
        }
        break;

      case STATE_WAIT_RELEASE_AFTER_TUNE:
        Serial.println("[FSM] -> ETAT: STATE_WAIT_RELEASE_AFTER_TUNE");
        leds.playValidation(100, 2);
        keyboard.getBaselineData(referenceBaselines);
        Serial.print("\n[CAL] HOLD detecte. Sensibilite validee. Cible: "); Serial.println(keyboard.getTargetBaseline());
        while(holdBtn.isPressed()){ holdBtn.read(); delay(5); }
        currentKey = 0;
        currentState = STATE_PREPARE_KEY;
        break;

      case STATE_PREPARE_KEY:
        Serial.println("[FSM] -> ETAT: STATE_PREPARE_KEY");
        if (currentKey == 0) {
            Serial.println("\n=== Phase 2: Calibration des Delta-Max ===");
        }
        currentMaxDelta = 0; lastPrintedMaxDelta = 0;
        Serial.print("\n[CAL] Mesure de la touche "); Serial.print(currentKey); Serial.println("... (Appuyez a fond, puis validez avec HOLD)");
        leds.displayStaticPattern(0, false);
        currentState = STATE_MEASURE_KEY;
        break;

      case STATE_MEASURE_KEY:
        {
          uint16_t currentFilteredData = keyboard.getFilteredData(currentKey);
          uint16_t delta = (referenceBaselines[currentKey] > currentFilteredData) ? (referenceBaselines[currentKey] - currentFilteredData) : 0;
          if (delta > currentMaxDelta) {
            currentMaxDelta = delta;
            if (currentMaxDelta > lastPrintedMaxDelta + 20) {
              Serial.print("  -> Nouveau max detecte: "); Serial.println(currentMaxDelta);
              lastPrintedMaxDelta = currentMaxDelta;
            }
          }
        }
        if (holdBtn.wasPressed()) {
          currentState = STATE_WAIT_RELEASE_AFTER_MEASURE;
        }
        break;

      case STATE_WAIT_RELEASE_AFTER_MEASURE:
        Serial.println("[FSM] -> ETAT: STATE_WAIT_RELEASE_AFTER_MEASURE");
        leds.playValidation(180, 1);
        measuredDeltas[currentKey] = currentMaxDelta;
        keyboard.setCalibrationMaxDelta(currentKey, currentMaxDelta);
        Serial.print("[CAL] Touche "); Serial.print(currentKey); Serial.print(" validee avec delta_max = "); Serial.println(currentMaxDelta);
        if (currentMaxDelta < CAL_PRESSURE_MIN_DELTA_TO_VALIDATE) Serial.println("  ATTENTION: delta_max est faible.");
        while(holdBtn.isPressed()){ holdBtn.read(); delay(5); }
        currentKey++;
        if (currentKey < NUM_KEYS) {
            currentState = STATE_PREPARE_KEY;
        } else {
            currentState = STATE_FINAL_CONFIRMATION;
        }
        break;

      case STATE_FINAL_CONFIRMATION:
        if (!recapDisplayed) {
          Serial.println("[FSM] -> ETAT: STATE_FINAL_CONFIRMATION");
          Serial.println("\n=== Phase 3: Confirmation Finale ===");
          Serial.print("Sensibilite choisie: ["); Serial.print(sensitivityNames[sensitivityIndex]);
          Serial.print("] (Cible: "); Serial.print(keyboard.getTargetBaseline()); Serial.println(")");
          Serial.println("\n--- Recapitulatif des Delta-Max Mesures ---");
          for (int i = 0; i < NUM_KEYS; ++i) {
            Serial.print(measuredDeltas[i]);
            if (i != NUM_KEYS - 1) {
              Serial.print((i % 12 == 11) ? "\n" : "\t");
            }
          }
          Serial.println("\n-------------------------------------------");
          Serial.println("\n[ACTION] Appuyer sur HOLD pour Sauvegarder et Quitter.");
          Serial.println("[ACTION] Appuyer sur MODE pour Recommencer la Calibration.");
          recapDisplayed = true;
        }

        if (holdBtn.wasPressed()) {
          currentState = STATE_SAVE_EXIT;
        }
        if (modeBtn.wasPressed()) {
          currentState = STATE_INIT;
        }
        break;

      case STATE_SAVE_EXIT:
        Serial.println("[FSM] -> ETAT: STATE_SAVE_EXIT");
        Serial.println("\n[CAL] Calibration terminee.");
        keyboard.calculateAdaptiveThresholds();
        leds.playValidation(180, 3);
        keyboard.saveCalibrationData();
        Serial.println("[CAL] SAVE: EEPROM ok. Retour au JEU.");
        leds.exitCalibrationMode();
        currentState = STATE_FINISHED;
        break;
        
      case STATE_FINISHED: 
        Serial.println("[FSM] -> ETAT: STATE_FINISHED");
        break;
    }
    delay(5);
  }
}