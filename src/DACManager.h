#ifndef DAC_MANAGER_H
#define DAC_MANAGER_H

#include <stdint.h>
#include <Wire.h>

// Using built-in Wire1 object for Nano R4 Qwiic connector
// No extern declaration needed

class DFRobot_GP8403;

class DACManager {
public:
  DACManager();
  ~DACManager();

  /**
   * @brief Initialise le DAC et le bus I2C.
   * @param i2cAddr L'adresse I2C du composant DAC.
   * @param wirePort Le port I2C à utiliser (Wire1 pour Qwiic).
   * @return true si l'initialisation est réussie, false sinon.
   */
  bool begin(uint8_t i2cAddr, TwoWire &wirePort);

  /**
   * @brief Définit la tension de sortie pour un canal donné.
   * @param channel Le canal à modifier (0 pour Pitch, 1 pour Aux).
   * @param voltage La tension cible en Volts.
   */
  void setOutputVoltage(uint8_t channel, float voltage);

private:
  DFRobot_GP8403* _dac;
};

#endif // DAC_MANAGER_H