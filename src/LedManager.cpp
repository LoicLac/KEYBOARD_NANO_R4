#include "LedManager.h"
#include "HardwareConfig.h"
#include <Arduino.h>

// Pins
const uint8_t LED_PINS[] = {PIN_LED_mm, PIN_LED_m, PIN_LED_c, PIN_LED_p, PIN_LED_pp};
const int NUM_LEDS = sizeof(LED_PINS) / sizeof(LED_PINS[0]);

// Timings
const uint32_t FX_MIN_VISIBILITY_MS = 150;
const uint32_t BARGRAPH_EXPIRY_MS   = 1000;
const uint32_t BARGRAPH_RATE_LIMIT_MS = 30;
const uint32_t STATIC_BLINK_PERIOD_MS = 150;

// =================================================================
// Lifecycle
// =================================================================
LedManager::LedManager() {
  currentMode = MODE_GAME;
  currentBackground = BG_OCTAVE_BREATHE;
  currentFX = FX_NONE;
  requestedFX = FX_NONE;

  bg_octaveIndex = 2;
  bg_period_ms = 2000;
  bg_pattern = 0;
  bg_blink = false;
  bg_brightness = 255;

  bg_startTime = 0;
  fx_startTime = 0;
  fx_minVisibleUntil = 0;
  bargraph_lastUpdateTime = 0;
  bargraph_lastRenderTime = 0;
}

void LedManager::begin() {
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }
  setAllLedsOff();
  bg_startTime = millis();
}

void LedManager::enterCalibrationMode() {
  currentMode = MODE_CALIBRATION;
  currentFX = FX_NONE;
  requestedFX = FX_NONE;
  currentBackground = BG_NONE;
  setAllLedsOff();
}

void LedManager::exitCalibrationMode() {
  currentMode = MODE_GAME;
  currentFX = FX_NONE;
  requestedFX = FX_NONE;
  displayOctave(bg_octaveIndex - 2, bg_period_ms);
}

// =================================================================
// Public API
// =================================================================
void LedManager::displayOctave(int octave, uint16_t period_ms) {
  if (currentMode != MODE_GAME) return;
  currentBackground = BG_OCTAVE_BREATHE;
  bg_octaveIndex = constrain(octave + 2, 0, NUM_LEDS - 1);
  bg_period_ms = period_ms;
  bg_startTime = millis();
}

void LedManager::displayBargraph(int percentage) {
  if (currentMode != MODE_GAME) return;
  requestedFX = FX_BARGRAPH;
  req_fx_param_bargraph_value = constrain(percentage, 0, 100);
}

void LedManager::displayInvertedBargraph(int percentage) {
  if (currentMode != MODE_GAME) return;
  requestedFX = FX_INVERTED_BARGRAPH;
  req_fx_param_bargraph_value = constrain(percentage, 0, 100);
}

void LedManager::displayStaticPattern(uint8_t pattern, bool blink, int brightness) {
  if (currentMode == MODE_GAME) {
    // En mode JEU, on change l'état de fond et on le laisse être géré par update()
    currentBackground = BG_STATIC_PATTERN;
    bg_pattern = pattern;
    bg_blink = blink;
    bg_brightness = constrain(brightness, 0, 255);
    bg_startTime = millis();
  } else { 
    // En mode CALIBRATION, on force l'affichage immédiat (pour les retours instantanés)
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((pattern >> i) & 0x01) {
        analogWrite(LED_PINS[i], brightness);
      } else {
        analogWrite(LED_PINS[i], 0);
      }
    }
  }
}

void LedManager::playValidation(uint16_t period_ms, uint8_t reps) {
  requestedFX = FX_VALIDATION;
  req_fx_param_period = period_ms;
  req_fx_param_reps = reps;
}

void LedManager::playChase(uint16_t step_ms, uint8_t reps) {
  requestedFX = FX_CHASE;
  req_fx_param_period = step_ms;
  req_fx_param_reps = reps;
}

void LedManager::playCrossfade(uint16_t period_ms, uint8_t reps) {
  requestedFX = FX_CROSSFADE;
  req_fx_param_period = period_ms;
  req_fx_param_reps = reps;
}

void LedManager::playInwardWipe(uint16_t step_ms, uint8_t reps) {
  requestedFX = FX_INWARD_WIPE;
  req_fx_param_period = step_ms;
  req_fx_param_reps = reps;
}

void LedManager::playCountdown(uint16_t duration_ms) {
  setAllLedsOff();
  if (duration_ms == 0) return;
  uint16_t step_duration = duration_ms / NUM_LEDS;
  for (int i = NUM_LEDS - 1; i >= 0; i--) {
    analogWrite(LED_PINS[i], 255);
    delay(step_duration);
  }
  setAllLedsOff();
}

// =================================================================
// Engine
// =================================================================
void LedManager::update() {
  unsigned long currentTime = millis();

  bool isBargraphRefresh = ((requestedFX == FX_BARGRAPH || requestedFX == FX_INVERTED_BARGRAPH) && (currentFX == FX_BARGRAPH || currentFX == FX_INVERTED_BARGRAPH));
  if (requestedFX != FX_NONE && !isBargraphRefresh && currentTime >= fx_minVisibleUntil) {
    startNewEffect(currentTime);
  }

  applyAndRender(currentTime);
}

void LedManager::startNewEffect(unsigned long currentTime) {
  currentFX = requestedFX;
  requestedFX = FX_NONE;

  fx_startTime = currentTime;
  fx_minVisibleUntil = currentTime + FX_MIN_VISIBILITY_MS;

  fx_param_period = req_fx_param_period;
  fx_param_reps = req_fx_param_reps;

  if (currentFX == FX_BARGRAPH || currentFX == FX_INVERTED_BARGRAPH) {
    fx_param_bargraph_value = req_fx_param_bargraph_value;
    bargraph_lastUpdateTime = currentTime;
    bargraph_lastRenderTime = 0;
  }
}

void LedManager::renderBackground(unsigned long currentTime) {
  unsigned long bg_elapsedTime = currentTime - bg_startTime;

  switch (currentBackground) {
    case BG_OCTAVE_BREATHE: {
      float sine_wave = sin(bg_elapsedTime * (TWO_PI / bg_period_ms));
      float normalized_wave = pow((sine_wave + 1.0f) * 0.5f, 2.0f);
      uint8_t brightness = LED_OCTAVE_BREATHE_MIN_BRIGHTNESS
                         + (uint8_t)(normalized_wave * (255 - LED_OCTAVE_BREATHE_MIN_BRIGHTNESS));
      for (int i = 0; i < NUM_LEDS; i++) {
        analogWrite(LED_PINS[i], (i == bg_octaveIndex) ? brightness : 0);
      }
      break;
    }
    case BG_STATIC_PATTERN: {
      bool isOn = true;
      if (bg_blink) {
        isOn = (bg_elapsedTime % STATIC_BLINK_PERIOD_MS) < (STATIC_BLINK_PERIOD_MS / 2);
      }
      if (!isOn) {
        setAllLedsOff();
      } else {
        for (int i = 0; i < NUM_LEDS; i++) {
          if ((bg_pattern >> i) & 0x01) {
            analogWrite(LED_PINS[i], bg_brightness);
          } else {
            analogWrite(LED_PINS[i], 0);
          }
        }
      }
      break;
    }
    case BG_NONE:
      setAllLedsOff();
      break;
  }
}

void LedManager::applyAndRender(unsigned long currentTime) {
  if (currentFX == FX_NONE) {
    renderBackground(currentTime);
    return;
  }

  unsigned long elapsedTime = currentTime - fx_startTime;
  bool effectFinished = false;

  switch (currentFX) {
    case FX_VALIDATION: {
      uint32_t totalDuration = (uint32_t)fx_param_reps * fx_param_period;
      if (elapsedTime >= totalDuration) { effectFinished = true; break; }
      bool isOn = (elapsedTime % fx_param_period) < (fx_param_period / 2);
      setAllLedsOff();
      analogWrite(LED_PINS[1], isOn ? 255 : 0);
      analogWrite(LED_PINS[2], isOn ? 255 : 0);
      analogWrite(LED_PINS[3], isOn ? 255 : 0);
      break;
    }
    case FX_CHASE: {
      uint32_t sequence_len = (NUM_LEDS > 1) ? (NUM_LEDS - 1) * 2 : 1;
      uint32_t totalDuration = (uint32_t)fx_param_reps * sequence_len * fx_param_period;
      if (elapsedTime >= totalDuration) { effectFinished = true; break; }
      int currentStep = (elapsedTime / fx_param_period);
      int stepInSequence = currentStep % sequence_len;
      int ledIndex = (stepInSequence < NUM_LEDS) ? stepInSequence : sequence_len - stepInSequence;
      for (int i = 0; i < NUM_LEDS; i++) {
        analogWrite(LED_PINS[i], (i == ledIndex) ? 255 : 0);
      }
      break;
    }
    case FX_BARGRAPH: {
      if (currentTime - bargraph_lastUpdateTime > BARGRAPH_EXPIRY_MS) { effectFinished = true; break; }
      if (requestedFX == FX_BARGRAPH || requestedFX == FX_INVERTED_BARGRAPH) {
        fx_param_bargraph_value = req_fx_param_bargraph_value;
        bargraph_lastUpdateTime = currentTime;
        requestedFX = FX_NONE;
      }
      if (currentTime - bargraph_lastRenderTime >= BARGRAPH_RATE_LIMIT_MS) {
        bargraph_lastRenderTime = currentTime;
        float level = (float)fx_param_bargraph_value / (100.0f / NUM_LEDS);
        for (int i = 0; i < NUM_LEDS; i++) {
          if (level >= 1.0f) {
            analogWrite(LED_PINS[i], 255);
          } else if (level > 0.0f) {
            analogWrite(LED_PINS[i], (int)(level * 255.0f));
          } else {
            analogWrite(LED_PINS[i], 0);
          }
          level -= 1.0f;
        }
      }
      break;
    }
    case FX_INVERTED_BARGRAPH: {
      if (currentTime - bargraph_lastUpdateTime > BARGRAPH_EXPIRY_MS) { effectFinished = true; break; }
      if (requestedFX == FX_BARGRAPH || requestedFX == FX_INVERTED_BARGRAPH) {
        fx_param_bargraph_value = req_fx_param_bargraph_value;
        bargraph_lastUpdateTime = currentTime;
        requestedFX = FX_NONE;
      }
      if (currentTime - bargraph_lastRenderTime >= BARGRAPH_RATE_LIMIT_MS) {
        bargraph_lastRenderTime = currentTime;
        // Fixed: Use full range of LEDs (0 to NUM_LEDS-1 = 0 to 4)
        // Map 0-100% to 0-4 LED positions
        float floatIndex = (float)fx_param_bargraph_value / 100.0f * (float)NUM_LEDS;
        int offLedIndex = constrain((int)floatIndex, 0, NUM_LEDS - 1);
        
        for (int i = 0; i < NUM_LEDS; i++) {
          if (i == offLedIndex) {
            analogWrite(LED_PINS[i], 0);
          } else {
            analogWrite(LED_PINS[i], 255);
          }
        }
      }
      break;
    }
    case FX_CROSSFADE: {
        uint32_t totalDuration = (uint32_t)fx_param_reps * fx_param_period;
        if (elapsedTime >= totalDuration) { effectFinished = true; break; }
        float sine_wave = sin(elapsedTime * (TWO_PI / fx_param_period));
        uint8_t brightnessA = (uint8_t)(((sine_wave + 1.0f) * 0.5f) * 255.0f);
        uint8_t brightnessB = 255 - brightnessA;
        setAllLedsOff();
        analogWrite(LED_PINS[0], brightnessA);
        analogWrite(LED_PINS[4], brightnessB);
        break;
    }
    case FX_INWARD_WIPE: {
        uint32_t sequence_len = 3;
        uint32_t totalDuration = (uint32_t)fx_param_reps * sequence_len * fx_param_period;
        if (elapsedTime >= totalDuration) { effectFinished = true; break; }
        int step = (elapsedTime / fx_param_period) % sequence_len;
        setAllLedsOff();
        if (step == 0) {
            analogWrite(LED_PINS[0], 255);
            analogWrite(LED_PINS[4], 255);
        } else if (step == 1) {
            analogWrite(LED_PINS[1], 255);
            analogWrite(LED_PINS[3], 255);
        } else {
            analogWrite(LED_PINS[2], 255);
        }
        break;
    }
    case FX_NONE:
      break;
  }

  if (effectFinished) {
    currentFX = FX_NONE;
    renderBackground(currentTime);
  }
}

void LedManager::setAllLedsOff() {
  for (int i = 0; i < NUM_LEDS; i++) {
    analogWrite(LED_PINS[i], 0);
  }
}