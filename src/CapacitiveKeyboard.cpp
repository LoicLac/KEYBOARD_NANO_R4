#include "CapacitiveKeyboard.h"
#include "HardwareConfig.h"
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <string.h>
#include <math.h>

#define MPR121_ECR          0x5E
#define MPR121_AUTOCONFIG0  0x7B
#define MPR121_USL          0x7D
#define MPR121_LSL          0x7E
#define MPR121_TL           0x7F

void CapacitiveKeyboard::writeRegister(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(addr);
  Wire1.write(reg);
  Wire1.write(value);
  Wire1.endTransmission();
}

uint8_t CapacitiveKeyboard::readRegister(uint8_t addr, uint8_t reg) {
  Wire1.beginTransmission(addr);
  Wire1.write(reg);
  Wire1.endTransmission(false);
  Wire1.requestFrom(addr, (uint8_t)1);
  return Wire1.read();
}

CapacitiveKeyboard::CapacitiveKeyboard() {
  isInitialized = false;
  responseShape = 0.5f;
  currentTargetBaseline = 550;
  aftertouchDeadzoneOffset = 0; // Initialisation de la nouvelle variable

  for (int i = 0; i < NUM_KEYS; i++) {
    filteredData[i] = 0;
    baselineData[i] = 0;
    smoothedPressure[i] = 0.0f;
    slewedPressure[i] = 0.0f;
    keyIsPressed[i] = false;
    lastKeyIsPressed[i] = false;
    calibrationMaxDelta[i] = 400;
    pressDeltaStart[i] = 0;
    historyIndex[i] = 0;
    for (int j = 0; j < AFTERTOUCH_SMOOTHING_WINDOW_SIZE; j++) {
      pressureHistory[i][j] = 0.0f;
    }
  }
}

bool CapacitiveKeyboard::begin() {
  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: Demarrage initialisation clavier capacitif...");
  #endif
  
  loadCalibrationData();
  #if DEBUG_LEVEL >= 0
    Serial.println("INFO: Calibration EEPROM chargee.");
  #endif
  delay(50);  // Allow calibration data to settle in memory
  
  #if DEBUG_LEVEL >= 0
  Serial.print("INFO: Lancement autoconfiguration MPR121 (cible: ");
  Serial.print(currentTargetBaseline);
  Serial.println(")...");
  #endif
  
  bool success = runAutoconfiguration(currentTargetBaseline);
  delay(100);  // Additional stabilization after autoconfiguration
  
  #if DEBUG_LEVEL >= 0
    if(success) {
      Serial.println("INFO: Capteurs capacitifs initialises avec succes.");
    } else {
      Serial.println("FATAL: Echec initialisation capteurs capacitifs.");
    }
  #endif
  return success;
}

void CapacitiveKeyboard::setResponseShape(float shape) {
    if (shape < 0.0f) shape = 0.0f;
    if (shape > 1.0f) shape = 1.0f;
    responseShape = shape;
}

void CapacitiveKeyboard::update() {
  if (!isInitialized) return;
  memcpy(lastKeyIsPressed, keyIsPressed, sizeof(keyIsPressed));
  pollAllSensorData();

  for (int i = 0; i < NUM_KEYS; i++) {
    uint16_t delta = (baselineData[i] > filteredData[i]) ? (baselineData[i] - filteredData[i]) : 0;

    // --- NOUVELLE MACHINE A ETATS POUR NOTE ON/OFF ---
    if (!keyIsPressed[i] && delta > pressThresholds[i]) {
      // --- EVENEMENT NOTE ON ---
      keyIsPressed[i] = true;
      pressDeltaStart[i] = delta; // Capture du "Zéro Relatif"
    } 
    else if (keyIsPressed[i] && delta < releaseThresholds[i]) {
      // --- EVENEMENT NOTE OFF ---
      keyIsPressed[i] = false;
      // "Retour à Zéro Forcé" : on réinitialise tout l'état de pression
      slewedPressure[i] = 0.0f;
      smoothedPressure[i] = 0.0f;
      for (int j = 0; j < AFTERTOUCH_SMOOTHING_WINDOW_SIZE; j++) {
        pressureHistory[i][j] = 0.0f;
      }
    }

    // --- Calcul de la pression si la touche est active ---
    float targetPressure = 0.0f;
    if (keyIsPressed[i]) {
      // La plage de pression utile va maintenant du delta de départ au delta max calibré
      uint16_t maxD = calibrationMaxDelta[i];
      uint16_t pressD = pressDeltaStart[i] + aftertouchDeadzoneOffset;     
       float aftertouch_norm = 0.0f;
      
      if (maxD > pressD) {
        // Calcul de la pression normalisée à partir du "Zéro Relatif"
        aftertouch_norm = (float)((delta > pressD) ? (delta - pressD) : 0) / (float)(maxD - pressD);
      }
      if (aftertouch_norm > 1.0f) aftertouch_norm = 1.0f;

      float shaped_norm = 0.0f;
      float x = aftertouch_norm;

      if (responseShape < 0.5f) {
        float y_exp = powf(x, AFTERTOUCH_CURVE_EXP_INTENSITY);
        float y_lin = x;
        float t = 1.0f - (responseShape * 2.0f);
        shaped_norm = y_lin * (1.0f - t) + y_exp * t;
      } else {
        float y_sig = x * x * (3.0f - 2.0f * x);
        for (int j = 1; j < AFTERTOUCH_CURVE_SIG_INTENSITY; j++) {
            y_sig = y_sig * y_sig * (3.0f - 2.0f * y_sig);
        }
        float y_lin = x;
        float t = (responseShape - 0.5f) * 2.0f;
        shaped_norm = y_lin * (1.0f - t) + y_sig * t;
      }
      targetPressure = shaped_norm * (float)CV_OUTPUT_RESOLUTION;
    }
    
    // --- Lissage ETAGE 1: Slew Limiter ---
    float diff = targetPressure - slewedPressure[i];
    if (diff > AFTERTOUCH_SLEW_RATE_LIMIT) {
      slewedPressure[i] += AFTERTOUCH_SLEW_RATE_LIMIT;
    } else if (diff < -AFTERTOUCH_SLEW_RATE_LIMIT) {
      slewedPressure[i] -= AFTERTOUCH_SLEW_RATE_LIMIT;
    } else {
      slewedPressure[i] = targetPressure;
    }

    // --- Lissage ETAGE 2: Moyenne Mobile ---
    pressureHistory[i][historyIndex[i]] = slewedPressure[i];
    historyIndex[i] = (historyIndex[i] + 1) % AFTERTOUCH_SMOOTHING_WINDOW_SIZE;

    float sum = 0.0f;
    for (int j = 0; j < AFTERTOUCH_SMOOTHING_WINDOW_SIZE; j++) {
      sum += pressureHistory[i][j];
    }
    smoothedPressure[i] = sum / (float)AFTERTOUCH_SMOOTHING_WINDOW_SIZE;
  }
}

bool CapacitiveKeyboard::isPressed(uint8_t note) { if (note >= NUM_KEYS) return false; return keyIsPressed[note]; }
bool CapacitiveKeyboard::noteOn(uint8_t note)    { if (note >= NUM_KEYS) return false; return keyIsPressed[note] && !lastKeyIsPressed[note]; }
bool CapacitiveKeyboard::noteOff(uint8_t note)   { if (note >= NUM_KEYS) return false; return !keyIsPressed[note] && lastKeyIsPressed[note]; }
uint16_t CapacitiveKeyboard::getPressure(uint8_t note) { if (note >= NUM_KEYS) return 0; return (uint16_t)smoothedPressure[note]; }
const bool* CapacitiveKeyboard::getPressedKeysState() const { return keyIsPressed; }

bool CapacitiveKeyboard::initializeHardware() {
  // Wire1 is already initialized in main setup()
  // No need to re-initialize here
  return true;
}

bool CapacitiveKeyboard::runAutoconfiguration(uint16_t targetBaseline) {
  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: Calcul parametres autoconfig MPR121...");
  #endif
  
  uint8_t tl = (targetBaseline / 4);
  uint8_t usl = (targetBaseline * 1.1f / 4); 
  uint8_t lsl = (targetBaseline * 0.7f / 4);
  
  #if DEBUG_LEVEL >= 0
  Serial.print("INFO: Parametres calcules - TL: ");
  Serial.print(tl);
  Serial.print(", USL: ");
  Serial.print(usl);
  Serial.print(", LSL: ");
  Serial.println(lsl);
  #endif

  uint8_t sensor_addrs[] = { ADDR_MPR121_A, ADDR_MPR121_B };
  
  for (int sensorIndex = 0; sensorIndex < 2; sensorIndex++) {
    uint8_t addr = sensor_addrs[sensorIndex];
    
    #if DEBUG_LEVEL >= 0
    Serial.print("INFO: Configuration capteur MPR121 #");
    Serial.print(sensorIndex + 1);
    Serial.print(" a l'adresse 0x");
    Serial.print(addr, HEX);
    Serial.println("...");
    #endif
    
    // Test I2C connection first
    Wire1.beginTransmission(addr);
    if (Wire1.endTransmission() != 0) { 
      #if DEBUG_LEVEL >= 0
      Serial.print("FATAL: Capteur MPR121 a l'adresse 0x");
      Serial.print(addr, HEX);
      Serial.println(" non trouve !");
      #endif
      return false; 
    }
    
    #if DEBUG_LEVEL >= 0
    Serial.println("INFO: Capteur detecte, arret du mode configuration...");
    #endif
    writeRegister(addr, MPR121_ECR, 0x00);  // Stop config mode
    delay(20);  // Allow config mode to stop
    
    #if DEBUG_LEVEL >= 0
    Serial.println("INFO: Configuration seuils touch/release (12 canaux)...");
    #endif
    for (uint8_t i = 0; i < 12; i++) {
        writeRegister(addr, 0x41 + 2 * i, 12);  // Touch threshold
        writeRegister(addr, 0x42 + 2 * i, 6);   // Release threshold
    }
    delay(10);  // Allow threshold registers to settle
    
    #if DEBUG_LEVEL >= 0
    Serial.println("INFO: Configuration parametres autoconfig...");
    #endif
    writeRegister(addr, 0x5B, 0);  // ACCR0 - Auto Config Control 0
    writeRegister(addr, MPR121_USL, usl);     // Upper Side Limit
    writeRegister(addr, MPR121_LSL, lsl);     // Lower Side Limit
    writeRegister(addr, MPR121_TL, tl);       // Target Level
    delay(10);  // Allow autoconfig parameters to settle
    
    #if DEBUG_LEVEL >= 0
    Serial.println("INFO: Activation autoconfig...");
    #endif
    writeRegister(addr, MPR121_AUTOCONFIG0, 0x0B);  // Enable autoconfig
    delay(20);  // Allow autoconfig to be enabled
    
    #if DEBUG_LEVEL >= 0
    Serial.println("INFO: Demarrage mode run...");
    #endif
    writeRegister(addr, MPR121_ECR, 0x0C);  // Start run mode (12 electrodes)
    delay(50);  // Allow run mode to start and stabilize
    
    #if DEBUG_LEVEL >= 0
    Serial.print("INFO: Capteur MPR121 #");
    Serial.print(sensorIndex + 1);
    Serial.println(" configure avec succes");
    #endif
  }
  
  currentTargetBaseline = targetBaseline;
  isInitialized = true;
  
  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: Stabilisation finale des capteurs...");
  #endif
  delay(100);  // Final stabilization
  
  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: Premier scan des donnees capteurs...");
  #endif
  pollAllSensorData();
  delay(50);  // Allow first data poll to complete
  
  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: Autoconfiguration MPR121 terminee avec succes");
  #endif
  
  return true;
}

void CapacitiveKeyboard::pollAllSensorData() {
  const uint8_t START_REGISTER = 0x04;
  const uint8_t BYTES_TO_READ  = 38;
  uint8_t sensor_addrs[] = { ADDR_MPR121_A, ADDR_MPR121_B };
  int keyOffset = 0;

  for(uint8_t addr : sensor_addrs) {
    Wire1.beginTransmission(addr);
    Wire1.write(START_REGISTER);
    Wire1.endTransmission(false);
    if (Wire1.requestFrom(addr, BYTES_TO_READ) == BYTES_TO_READ) {
      for (int i = 0; i < 12; i++) { filteredData[i + keyOffset] = Wire1.read() | (Wire1.read() << 8); }
      Wire1.read(); Wire1.read();
      for (int i = 0; i < 12; i++) { baselineData[i + keyOffset] = Wire1.read() << 2; }
    }
    keyOffset += 12;
  }
}

void CapacitiveKeyboard::saveCalibrationData() {
  CalDataStore data;
  data.magic = EEPROM_MAGIC;
  data.version = EEPROM_VERSION;
  data.reserved = 0;
  data.target_baseline = currentTargetBaseline;
  memcpy(data.maxDelta, calibrationMaxDelta, sizeof(calibrationMaxDelta));
  EEPROM.put(0, data);
}

void CapacitiveKeyboard::loadCalibrationData() {
  CalDataStore data;
  EEPROM.get(0, data);
  if (data.magic == EEPROM_MAGIC && data.version == EEPROM_VERSION) {
    memcpy(calibrationMaxDelta, data.maxDelta, sizeof(calibrationMaxDelta));
    currentTargetBaseline = data.target_baseline;
    #if DEBUG_LEVEL >= 0
      // Ce message est déjà présent dans begin(), pas besoin de le dupliquer.
    #endif
  } else {
    // NOUVEAU BLOC D'ALERTE
    #if DEBUG_LEVEL >= 0
      Serial.println("------------------------------------------------------------");
      Serial.println("ATTENTION: Aucune calibration valide trouvee en EEPROM.");
      Serial.println("Le clavier utilise les reglages par defaut.");
      Serial.println("Il est fortement recommande d'effectuer une calibration.");
      Serial.println("------------------------------------------------------------");
    #endif
  }
  calculateAdaptiveThresholds();
}
void CapacitiveKeyboard::setAftertouchDeadzone(int offset) {
  aftertouchDeadzoneOffset = constrain(offset, 0, AFTERTOUCH_DEADZONE_MAX_OFFSET);
}
void CapacitiveKeyboard::calculateAdaptiveThresholds() {
  for (int i=0; i<NUM_KEYS; i++) {
    uint16_t pressT = calibrationMaxDelta[i] * PRESS_THRESHOLD_PERCENT;
    uint16_t releaseT = calibrationMaxDelta[i] * RELEASE_THRESHOLD_PERCENT;
    pressThresholds[i] = max(pressT, MIN_PRESS_THRESHOLD);
    releaseThresholds[i] = max(releaseT, MIN_RELEASE_THRESHOLD);
    if (releaseThresholds[i] >= pressThresholds[i]) {
      releaseThresholds[i] = pressThresholds[i] > 1 ? pressThresholds[i] - 1 : 0;
    }
  }
}

void CapacitiveKeyboard::logFullBaselineTable() {
  Serial.println("\n--- Baselines Actuelles ---");
  for (int i = 0; i < NUM_KEYS; ++i) {
    Serial.print(baselineData[i]);
    if (i != NUM_KEYS - 1) {
        Serial.print((i % 12 == 11) ? "\n" : "\t");
    }
  }
  Serial.println("\n---------------------------");
}

void CapacitiveKeyboard::getBaselineData(uint16_t* destArray) {
  memcpy(destArray, baselineData, sizeof(uint16_t) * NUM_KEYS);
}

uint16_t CapacitiveKeyboard::getFilteredData(int key) {
  if (key >= 0 && key < NUM_KEYS) {
    return filteredData[key];
  }
  return 0;
}

void CapacitiveKeyboard::setCalibrationMaxDelta(int key, uint16_t delta) {
  if (key >= 0 && key < NUM_KEYS) {
    calibrationMaxDelta[key] = delta;
  }
}

uint16_t CapacitiveKeyboard::getTargetBaseline() const {
  return currentTargetBaseline;
}