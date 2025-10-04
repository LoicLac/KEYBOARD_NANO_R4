#include "HardwareConfig.h"
#include "KeyboardData.h"
#include "CapacitiveKeyboard.h"
#include "KeyboardCalibrator.h"
#include "LedController.h"
#include "DACManager.h"
#include "EngineMode1.h"
#include "EngineMode2.h"
#include "EngineMode3.h"
#include "InputManager.h"
#include <JC_Button.h>
#include <Arduino.h>
#include <MIDI.h>
#include <Wire.h>

// Using built-in Wire1 object for Nano R4 Qwiic connector

// =================================================================
// 1. Instances des Objets & Etat Global
// =================================================================
CapacitiveKeyboard keyboard;
DACManager         dac;
InputManager       inputManager;
LedController      ledController;

EngineMode1 engine1;
EngineMode2 engine2;
EngineMode3 engine3;

GameMode currentMode = MODE_PRESSURE_GLIDE;

// Instances de boutons conservées UNIQUEMENT pour le KeyboardCalibrator
Button btnHold(PIN_BTN_HOLD, BUTTON_DEBOUNCE_MS);
Button btnMode(PIN_BTN_MODE, BUTTON_DEBOUNCE_MS);
Button btnOctPlus(PIN_BTN_OCT_PLUS, BUTTON_DEBOUNCE_MS);
Button btnOctMinus(PIN_BTN_OCT_MINUS, BUTTON_DEBOUNCE_MS);

MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDI);

// =================================================================
// 2. Fonctions de Rappel MIDI (Callbacks)
// =================================================================
void handleMidiNoteOn(byte channel, byte pitch, byte velocity) {
  if (currentMode == MODE_MIDI) {
    // engine3.onMidiNoteOn(pitch, velocity);
  }
}
void handleMidiNoteOff(byte channel, byte pitch, byte velocity) {
  if (currentMode == MODE_MIDI) {
    // engine3.onMidiNoteOff(pitch);
  }
}

// =================================================================
// 3. SETUP
// =================================================================
void setup() {
#if DEBUG_LEVEL >= 0
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {}
  Serial.println("\n===== Initialisation du Clavier Avance V9 =====");
  Serial.println("Plateforme: Arduino Nano R4 avec Qwiic");
#endif

  ledController.begin();
  pinMode(PIN_GATE, OUTPUT);
  pinMode(PIN_TRIGGER, OUTPUT);
  
  btnHold.begin();
  btnMode.begin();
  btnOctPlus.begin();
  btnOctMinus.begin();
  
  inputManager.begin();

#if DEBUG_LEVEL >= 0
  Serial.println("\n--- Initialisation I2C Bus ---");
#endif
  Wire1.begin();
#if DEBUG_LEVEL >= 0
  Serial.println("INFO: Wire1 I2C bus demarre");
#endif
  delay(50);  // Stabilization delay for I2C bus
  
  Wire1.setClock(I2C_CLOCK_HZ);
#if DEBUG_LEVEL >= 0
  Serial.print("INFO: Horloge I2C reglee a ");
  Serial.print(I2C_CLOCK_HZ);
  Serial.println(" Hz");
#endif
  delay(100);  // Allow I2C clock to stabilize
  
  if (InputManager::isHoldPressedOnBoot()) {
    LedManager tempLedManager;
    tempLedManager.begin();
    KeyboardCalibrator calibrator;
    calibrator.run(keyboard, tempLedManager, dac, btnHold, btnMode, btnOctPlus, btnOctMinus);
  }
  
#if DEBUG_LEVEL >= 0
  Serial.println("\n--- Verification Presence Peripheriques I2C ---");
  // Quick device detection before initialization
  Wire1.beginTransmission(0x5A);
  if (Wire1.endTransmission() == 0) Serial.println("INFO: MPR121 Sensor A detecte (0x5A)");
  else Serial.println("WARN: MPR121 Sensor A non trouve (0x5A)");
  
  Wire1.beginTransmission(0x5B);
  if (Wire1.endTransmission() == 0) Serial.println("INFO: MPR121 Sensor B detecte (0x5B)");
  else Serial.println("WARN: MPR121 Sensor B non trouve (0x5B)");
  
  Wire1.beginTransmission(DAC_I2C_ADDR);
  if (Wire1.endTransmission() == 0) Serial.println("INFO: DAC GP8403 detecte (0x5F)");
  else Serial.println("WARN: DAC GP8403 non trouve (0x5F)");
  
  Serial.println("\n--- Initialisation Peripheriques I2C ---");
#endif
  bool dacOK = dac.begin(DAC_I2C_ADDR, Wire1);
  delay(200);  // Give DAC time to fully initialize
  
  bool kbdOK = keyboard.begin();
  delay(200);  // Give MPR121 sensors time to fully initialize
  
  if (!dacOK || !kbdOK) { 
#if DEBUG_LEVEL >= 0
    Serial.println("FATAL: Echec initialisation peripheriques I2C");
    Serial.print("DAC OK: "); Serial.println(dacOK ? "Oui" : "Non");
    Serial.print("Clavier OK: "); Serial.println(kbdOK ? "Oui" : "Non");
#endif
    while(1) { /* Gestion erreur critique */ }
  }

  engine1.begin();
  engine2.begin();
  engine3.begin();
  
  MIDI.setHandleNoteOn(handleMidiNoteOn);
  MIDI.setHandleNoteOff(handleMidiNoteOff);
  MIDI.begin(MIDI_CHANNEL_OMNI);

  #if DEBUG_LEVEL >= 0
  Serial.println("Pret a jouer.");
  #endif
}

// =================================================================
// 4. Fonctions de Transition et de Rendu
// =================================================================

void transitionToMode(GameMode newMode) {
  if (newMode == currentMode) return;
  digitalWrite(PIN_GATE, LOW);
  digitalWrite(PIN_TRIGGER, LOW);
  dac.setOutputVoltage(1, 0.0f);
  currentMode = newMode;
}

void renderAudioOutputs(float pitchV, float auxV, bool gateState, bool retrigger) {
  dac.setOutputVoltage(0, pitchV);
  dac.setOutputVoltage(1, auxV);
  
  static unsigned long triggerEndTime = 0;
  static bool triggerActive = false;
  if (retrigger) {
    digitalWrite(PIN_TRIGGER, HIGH);
    triggerEndTime = millis() + TRIGGER_PULSE_DURATION_MS;
    triggerActive = true;
  }
  if (triggerActive && millis() >= triggerEndTime) {
    digitalWrite(PIN_TRIGGER, LOW);
    triggerActive = false;
  }
  digitalWrite(PIN_GATE, gateState);
}

// =================================================================
// 5. LOOP PRINCIPALE
// =================================================================
void loop() {
  inputManager.update();
  const InputEvents& events = inputManager.getEvents();
  keyboard.update();

  if (events.mode_wasPressedLong) {
    int nextModeIndex = ((int)currentMode + 1) % 3;
    transitionToMode((GameMode)nextModeIndex);
  }

  float pitchV, auxV;
  bool gateState, retrigger;
  const bool* physicalKeyState = keyboard.getPressedKeysState();

  switch (currentMode) {
    case MODE_PRESSURE_GLIDE: {
      engine1.processInputs(events, physicalKeyState);
      keyboard.setAftertouchDeadzone(engine1.getAftertouchDeadzoneOffset());
      for (int i = 0; i < NUM_KEYS; i++) {
        uint8_t pitch = 36 + i;
        if (keyboard.noteOn(i))  engine1.onNoteOn(pitch, keyboard.getPressure(i));
        if (keyboard.noteOff(i)) engine1.onNoteOff(pitch);
        if (keyboard.isPressed(i)) engine1.onAftertouchUpdate(i, keyboard.getPressure(i));
      }
      engine1.update();
      pitchV = engine1.getPitchVoltage();
      auxV = engine1.getAuxVoltage();
      gateState = engine1.getGateState();
      retrigger = engine1.getAndClearRetriggerEvent();
      break;
    }
    case MODE_INTERVAL: {
      engine2.processInputs(events, physicalKeyState);
      // Share aftertouch parameters from Engine1
      keyboard.setAftertouchDeadzone(engine1.getAftertouchDeadzoneOffset());
      engine2.setSharedAftertouchParams(engine1.getAuxSmoothingAlpha());
      for (int i = 0; i < NUM_KEYS; i++) {
        uint8_t pitch = 36 + i;
        if (keyboard.noteOn(i))  engine2.onNoteOn(pitch, keyboard.getPressure(i));
        if (keyboard.noteOff(i)) engine2.onNoteOff(pitch);
        if (keyboard.isPressed(i)) engine2.onAftertouchUpdate(i, keyboard.getPressure(i));
      }
      engine2.update();
      pitchV = engine2.getPitchVoltage();
      auxV = engine2.getAuxVoltage();
      gateState = engine2.getGateState();
      retrigger = engine2.getAndClearRetriggerEvent();
      break;
    }
    case MODE_MIDI: {
      engine3.processInputs(events, physicalKeyState);
      engine3.update();
      pitchV = engine3.getPitchVoltage();
      auxV = engine3.getAuxVoltage();
      gateState = engine3.getGateState();
      retrigger = engine3.getAndClearRetriggerEvent();
      break;
    }
  }

  // L'appel au LedController est maintenant à la fin pour lui donner le contexte final
  ledController.update(currentMode, events, engine1, engine2, engine3, keyboard);

  renderAudioOutputs(pitchV, auxV, gateState, retrigger);
}