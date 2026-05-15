#pragma once

//===========================================
//  Configuration for Monkey Ball Joystick
//===========================================

// Enable to print output logs to the serial interface through USB
static const bool SERIAL_OUTPUT = false;

// These were values that I measured on my own joystick when testing initially
// This is just a starting point for calibration. Use the calibration steps to change the numbers
// in EEPROM which persists always. If something happens to the EEPROM or it gets wiped
// these defaults act as a fallback
static const int DEFAULT_X_LEFT   = 443;
static const int DEFAULT_X_CENTER = 519;
static const int DEFAULT_X_RIGHT  = 593;

static const int DEFAULT_Y_UP     = 440;
static const int DEFAULT_Y_CENTER = 513;
static const int DEFAULT_Y_DOWN   = 590;

// Processing flags
static const bool ENABLE_CURVE      = true;
static const bool ENABLE_SMOOTHING  = true;
static const bool ENABLE_EMA        = true;

// Deadzone radius (circle, in digital units after converting from analog)
static const int   DEADZONE = 70;

// Tanh curve tightness (the larger the number, the tighter the center)
static const float TANH_K = 1.6f;

// EMA alpha (Range: 0 to 1): higher value means more responsive. Lower value means smoother, but more input lag.
// I reccomend not touching this. If things still feel late/laggy for you, increase the value closer to 1
static const float EMA_ALPHA = 0.9f;

// Enable if your joystick has inverted axes
static const bool  INVERT_X = true;
static const bool  INVERT_Y = true;

// Defines for the pins on the Arduino
const int PIN_X         = A0; // Pin 14
const int PIN_Y         = A1; // Pin 15

const int START_PIN     = 2;
const int START_INDEX   = 0; // HID button #1

const int HOTKEY_PIN    = 3;
const int HOTKEY_INDEX  = 1; // HID button #2

const int SERVICE_PIN   = 4;
const int SERVICE_INDEX = 2; // HID button #3

const int TEST_PIN      = 5;
const int TEST_INDEX    = 3; // HID button #4

const int COIN_PIN      = 6;
const int COIN_INDEX    = 4; // HID button #5

// Defines for Debounce logic
const uint32_t DEBOUNCE_MS = 1;

// I have a real coin mechanism in my arcade cabinet, so I needed special values
// specifically for the coin button. You can set the REAL_COIN_SLOT flag to false
// if you are using a regular button
static const bool REAL_COIN_SLOT = true;
const uint32_t COIN_DEBOUNCE_MS = 25;
const uint32_t COIN_PULSE_MS = 100;
const uint32_t COIN_COOLDOWN_MS = 100;

// How many axes samples to average per loop
static const int NUM_SAMPLES = (ENABLE_SMOOTHING ? 4 : 1);

// Customize the button that is used for entering calibration mode
const int    MULTITAP_BUTTON_PIN = HOTKEY_PIN;
const int    MULTITAP_BUTTON_INDEX = HOTKEY_INDEX;
const char*  MULTITAP_BUTTON_NAME = "HOTKEY";

// Config for entering calibration mode
// The Arduino will listen for consecutive button presses of the MULTITAP_BUTTON
// all pressed within the MULTITAP_MAX_GAP_MS window for each press
// If the last press is held for at least MULTITAP_HOLD_SEC time, then calibration mode enables
static const uint8_t  MULTITAP_COUNT = 8;
static const uint32_t MULTITAP_MAX_GAP_MS = 1200; // max gap between taps
static const uint32_t MULTITAP_HOLD_SEC = 3;

// Calibration mode states
enum Mode { MODE_NORMAL, MODE_CALIBRATING };
enum CalibrationStep { CAL_NONE, CAL_CENTER, CAL_Y_UP, CAL_Y_DOWN, CAL_X_RIGHT, CAL_X_LEFT, CAL_DONE };

// How frequent to poll for input
static const uint32_t LOOP_INTERVAL_US = 100;

// HID send rate limit (If it goes beyond 1000Hz (1ms), packets will start getting dropped
// at the interface level. This was fun to debug)
static const uint32_t HID_SEND_INTERVAL_MS = 2; // 500Hz

// Joystick HID axis range (don't touch)
static const int JOY_MIN = 0;
static const int JOY_MAX = 1023;
