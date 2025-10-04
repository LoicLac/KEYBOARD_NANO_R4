#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "HardwareConfig.h"

class LedManager {
public:
  LedManager();
  void begin();
  void update();
  
  void enterCalibrationMode();
  void exitCalibrationMode();

  // API Publique pour le mode JEU
  void displayOctave(int octave, uint16_t period_ms);
  void displayBargraph(int percentage);
  void displayInvertedBargraph(int percentage);
  
  // SIGNATURE MISE À JOUR: Ajout d'un paramètre de luminosité
  void displayStaticPattern(uint8_t pattern, bool blink, int brightness = 255);
  
  // API Publique pour les effets
  void playValidation(uint16_t period_ms, uint8_t reps);
  void playChase(uint16_t step_ms, uint8_t reps);
  void playCrossfade(uint16_t period_ms, uint8_t reps);
  void playInwardWipe(uint16_t step_ms, uint8_t reps);
  void playCountdown(uint16_t duration_ms);
  void playPatternDisplay(int patternIndex);

private:
  enum Mode { MODE_GAME, MODE_CALIBRATION };
  enum BackgroundType { BG_NONE, BG_OCTAVE_BREATHE, BG_STATIC_PATTERN };
  enum FxType { FX_NONE, FX_VALIDATION, FX_CHASE, FX_BARGRAPH, FX_CROSSFADE, FX_INWARD_WIPE, FX_INVERTED_BARGRAPH, FX_PATTERN_DISPLAY };

  Mode currentMode;
  BackgroundType currentBackground;
  FxType currentFX;
  FxType requestedFX;

  // Paramètres du background
  uint8_t  bg_octaveIndex;
  uint16_t bg_period_ms;
  uint8_t  bg_pattern;
  bool     bg_blink;
  int      bg_brightness; // NOUVEAU
  
  // Paramètres des effets
  uint16_t req_fx_param_period;
  uint8_t  req_fx_param_reps;
  int      req_fx_param_bargraph_value;
  int      req_fx_param_pattern_index;
  uint16_t fx_param_period;
  uint8_t  fx_param_reps;
  int      fx_param_bargraph_value;
  int      fx_param_pattern_index;

  // Timers
  unsigned long bg_startTime;
  unsigned long fx_startTime;
  unsigned long fx_minVisibleUntil;
  unsigned long bargraph_lastUpdateTime;
  unsigned long bargraph_lastRenderTime;

  // Moteur interne
  void startNewEffect(unsigned long currentTime);
  void renderBackground(unsigned long currentTime);
  void applyAndRender(unsigned long currentTime);
  void setAllLedsOff();
};
#endif // LED_MANAGER_H