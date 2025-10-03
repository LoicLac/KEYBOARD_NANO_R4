#ifndef CAPACITIVE_KEYBOARD_H
#define CAPACITIVE_KEYBOARD_H

#include "KeyboardData.h"
#include "HardwareConfig.h"
#include <stdint.h>
#include <Wire.h>

// Using built-in I2C bus - Nano R4 may not have Wire1 by default

class LedManager;
class Button;

class CapacitiveKeyboard {
public:
  CapacitiveKeyboard();

  // API principale
  bool begin();
  void update();

  // Getters pour l'état des notes
  bool isPressed(uint8_t note);
  bool noteOn(uint8_t note);
  bool noteOff(uint8_t note);
  uint16_t getPressure(uint8_t note);
  const bool* getPressedKeysState() const;

  void setResponseShape(float shape);
  
  // NOUVELLE METHODE: Règle la zone morte de l'aftertouch
  void setAftertouchDeadzone(int offset);

  // API pour Outils Externes
  bool initializeHardware();
  bool runAutoconfiguration(uint16_t targetBaseline);
  void pollAllSensorData();
  void saveCalibrationData();
  void loadCalibrationData();
  void calculateAdaptiveThresholds();
  void logFullBaselineTable();
  void getBaselineData(uint16_t* destArray);
  uint16_t getFilteredData(int key);
  void setCalibrationMaxDelta(int key, uint16_t delta);
  uint16_t getTargetBaseline() const;


private:
  void writeRegister(uint8_t addr, uint8_t reg, uint8_t value);
  uint8_t readRegister(uint8_t addr, uint8_t reg);

  uint16_t filteredData[NUM_KEYS];
  uint16_t baselineData[NUM_KEYS];
  float    smoothedPressure[NUM_KEYS];
  bool     keyIsPressed[NUM_KEYS];
  bool     lastKeyIsPressed[NUM_KEYS];
  bool     isInitialized;

  uint16_t currentTargetBaseline;
  uint16_t calibrationMaxDelta[NUM_KEYS];
  uint16_t pressThresholds[NUM_KEYS];
  uint16_t releaseThresholds[NUM_KEYS];
  
  float responseShape;

  // NOUVELLE VARIABLE: Stocke l'offset de la zone morte
  int aftertouchDeadzoneOffset;

  // Variables pour le lissage et la nouvelle logique de pression
  uint16_t pressDeltaStart[NUM_KEYS];
  float slewedPressure[NUM_KEYS];
  float pressureHistory[NUM_KEYS][AFTERTOUCH_SMOOTHING_WINDOW_SIZE];
  int   historyIndex[NUM_KEYS];
};

#endif