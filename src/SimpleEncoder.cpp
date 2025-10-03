#include "SimpleEncoder.h"

// Quadrature state transition table
// [current_state][new_state] = direction
// States: 0=00, 1=01, 2=10, 3=11
const int SimpleEncoder::QUAD_STATES[4][4] = {
  {0, -1,  1,  0},  // State 0: 00 -> valid transitions to 1(-1) or 2(+1)
  {1,  0,  0, -1},  // State 1: 01 -> valid transitions to 0(+1) or 3(-1)
  {-1, 0,  0,  1},  // State 2: 10 -> valid transitions to 0(-1) or 3(+1)
  {0,  1, -1,  0}   // State 3: 11 -> valid transitions to 1(+1) or 2(-1)
};
