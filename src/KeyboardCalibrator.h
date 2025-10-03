#ifndef KEYBOARD_CALIBRATOR_H
#define KEYBOARD_CALIBRATOR_H

// Forward declarations
class CapacitiveKeyboard;
class LedManager;
class Button;
class DACManager;

/**
 * @class KeyboardCalibrator
 * @brief Gère l'ensemble du processus de calibration interactive du clavier.
 */
class KeyboardCalibrator {
public:
  KeyboardCalibrator();

  /**
   * @brief Lance la routine de calibration complète.
   * @param keyboard Référence vers l'instance du moteur clavier à calibrer.
   * @param leds Référence vers le gestionnaire de LEDs pour le retour visuel.
   * @param dac Référence vers le gestionnaire de DAC pour le mettre à zéro.
   * @param holdBtn Référence vers le bouton HOLD.
   * @param modeBtn Référence vers le bouton MODE.
   * @param octPlusBtn Référence vers le bouton OCT+.
   * @param octMinusBtn Référence vers le bouton OCT-.
   */
  void run(
    CapacitiveKeyboard &keyboard, 
    LedManager &leds,
    DACManager &dac,
    Button &holdBtn, 
    Button &modeBtn, 
    Button &octPlusBtn, 
    Button &octMinusBtn
  );
};

#endif // KEYBOARD_CALIBRATOR_H