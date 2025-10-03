#ifndef SIMPLE_ENCODER_H
#define SIMPLE_ENCODER_H

#include <Arduino.h>

/**
 * Enhanced Rotary Encoder Class with Debouncing
 * 
 * Features:
 * - Hardware debouncing with configurable timeout
 * - Complete quadrature state machine
 * - Noise filtering and validation
 * - Configurable sensitivity
 */
class SimpleEncoder {
private:
  int _pinA, _pinB;
  int _lastStateA, _lastStateB;
  unsigned long _lastChangeTime;
  int _state;  // Current quadrature state (0-3)
  
  // Quadrature state machine - [current_state][new_state] = direction
  static const int QUAD_STATES[4][4];
  
public:
  // Expose debouncing time as public constant for easy configuration
  static const unsigned long DEFAULT_DEBOUNCE_TIME_MS = 2;
  unsigned long debounceTimeMs;
  
  SimpleEncoder(int pinA, int pinB, unsigned long debounceMs = DEFAULT_DEBOUNCE_TIME_MS) 
    : _pinA(pinA), _pinB(pinB), debounceTimeMs(debounceMs) {
    
    // Initialize pins with internal pull-ups
    pinMode(_pinA, INPUT_PULLUP);
    pinMode(_pinB, INPUT_PULLUP);
    
    // Read initial state
    _lastStateA = digitalRead(_pinA);
    _lastStateB = digitalRead(_pinB);
    _state = (_lastStateA << 1) | _lastStateB;  // Combine into state
    _lastChangeTime = millis();
  }
  
  /**
   * Read encoder movement with debouncing
   * @return +1 (clockwise), -1 (counter-clockwise), or 0 (no change)
   */
  int read() {
    int currentStateA = digitalRead(_pinA);
    int currentStateB = digitalRead(_pinB);
    unsigned long now = millis();
    
    // Debounce: ignore changes too close together
    if (now - _lastChangeTime < debounceTimeMs) {
      return 0;
    }
    
    // Check if state actually changed
    if (currentStateA == _lastStateA && currentStateB == _lastStateB) {
      return 0;
    }
    
    // Update state
    int newState = (currentStateA << 1) | currentStateB;
    int direction = QUAD_STATES[_state][newState];
    
    if (direction != 0) {
      _state = newState;
      _lastStateA = currentStateA;
      _lastStateB = currentStateB;
      _lastChangeTime = now;
      return direction;
    }
    
    return 0;
  }
  
  /**
   * Reset encoder state (useful for initialization)
   */
  void reset() {
    _lastStateA = digitalRead(_pinA);
    _lastStateB = digitalRead(_pinB);
    _state = (_lastStateA << 1) | _lastStateB;
    _lastChangeTime = millis();
  }
  
  /**
   * Set debounce time
   */
  void setDebounceTime(unsigned long ms) {
    debounceTimeMs = ms;
  }
  
  /**
   * Get current quadrature state (for debugging)
   */
  int getState() const {
    return _state;
  }
};


#endif // SIMPLE_ENCODER_H
