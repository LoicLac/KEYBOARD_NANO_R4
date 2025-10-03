#include "DACManager.h"
#include "HardwareConfig.h"
#include <DFRobot_GP8403.h>

DACManager::DACManager() {
  _dac = nullptr;
}

DACManager::~DACManager() {
  if (_dac) {
    delete _dac;
    _dac = nullptr;
  }
}

bool DACManager::begin(uint8_t i2cAddr, TwoWire &wirePort) {
  #if DEBUG_LEVEL >= 0
  Serial.print("INFO: Initialisation DAC GP8403 a l'adresse 0x");
  Serial.print(i2cAddr, HEX);
  Serial.println("...");
  #endif

  if (!_dac) {
    _dac = new DFRobot_GP8403(&wirePort, i2cAddr);
    #if DEBUG_LEVEL >= 0
    Serial.println("INFO: Instance DAC creee");
    #endif
  }
  
  delay(50);  // Allow DAC object to settle

  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: Tentative de connexion au DAC...");
  #endif
  
  // La méthode begin() de la bibliothèque DFRobot renvoie 0 en cas de succès.
  if (_dac->begin() != 0) {
    #if DEBUG_LEVEL >= 0
    Serial.print("FATAL: DAC GP8403 a l'adresse 0x");
    Serial.print(i2cAddr, HEX);
    Serial.println(" non trouve !");
    #endif
    return false;
  }
  
  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: DAC GP8403 detecte et connecte");
  #endif
  delay(100);  // Allow DAC hardware to stabilize after detection

  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: Configuration plage de sortie DAC (0-10V)...");
  #endif
  
  // Configure la plage de sortie pour les deux canaux à 0-10V.
  _dac->setDACOutRange(DFRobot_GP8403::eOutputRange10V);
  delay(50);  // Allow range configuration to take effect

  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: DAC GP8403 initialise avec succes (0-10V).");
  Serial.println("INFO: Test initial des sorties DAC (0V sur les deux canaux)...");
  #endif
  
  // Test initial - set both outputs to 0V
  _dac->setDACOutVoltage(0, 0);  // Channel 0 to 0V
  _dac->setDACOutVoltage(0, 1);  // Channel 1 to 0V
  delay(50);  // Allow initial output to stabilize
  
  #if DEBUG_LEVEL >= 0
  Serial.println("INFO: DAC pret pour utilisation");
  #endif
  
  return true;
}

void DACManager::setOutputVoltage(uint8_t channel, float voltage) {
  if (!_dac || channel > 1) {
    return;
  }

  // La bibliothèque attend une valeur en millivolts.
  float clampedVoltage = constrain(voltage, 0.0f, DAC_OUTPUT_VOLTAGE_RANGE);
  uint16_t millivolts = clampedVoltage * 1000;

  // Met à jour la tension dans le registre volatile (RAM) du DAC.
  _dac->setDACOutVoltage(millivolts, channel);
}