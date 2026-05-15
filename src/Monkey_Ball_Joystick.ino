//==================================================================================
//  Monkey Ball Joystick - Arduino Pro Micro to USB HID Gamepad
//  Version: 2.7
//  Author: ShellHg
//
//  Arduino Pinout:
//    |--------------------|--------------|-------------|
//    |   Button Function  | Arduino Pin# | HID Button# |
//    |--------------------|--------------|-------------|
//    |              START |        Pin 2 |      HID #1 |
//    |             HOTKEY |        Pin 3 |      HID #2 |
//    | SERVICE (X Button) |        Pin 4 |      HID #3 |
//    |    TEST (Y Button) |        Pin 5 |      HID #4 |
//    | SELECT (Coin Slot) |        Pin 6 |      HID #5 |
//    |    Joystick X Axis |           A0 | Left Axis X |
//    |    Joystick Y Axis |           A1 | Left Axis Y |
//    |--------------------|--------------|-------------|
//
//  Calibration (Persists in EEPROM):
//    Enter by pressing MULTITAP_BUTTON MULTITAP_COUNT times in a row, holding the last for MULTITAP_HOLD_SEC seconds
//    If you want to abort, hold MULTITAP_BUTTON for MULTITAP_HOLD_SEC seconds at any calibration step.
//    The calibration order is CENTER -> UP -> DOWN -> RIGHT -> LEFT (press MULTITAP_BUTTON at each step)
//
//==================================================================================

#include <Arduino.h>
#include <Joystick.h>
#include <math.h>
#include <EEPROM.h>

// USB HID Joystick: 5 buttons, X and Y axes, manual send mode
Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID, 
                    JOYSTICK_TYPE_JOYSTICK,
                    5,                          // 5 buttons
                    0,                          // 0 hat switches
                    true,                       // X Axis
                    true,                       // Y Axis
                    false,                      // No Z Axis
                    false,                      // No Rx Axis
                    false,                      // No Ry Axis
                    false,                      // No Rz Axis
                    false,                      // No Rudder
                    false,                      // No Throttle
                    false,                      // No Accelerator
                    false,                      // No Brake
                    false);                     // No Steering

#include "config.h"

// Global runtime calibration values
static int X_LEFT_RAW;
static int X_CENTER_RAW;
static int X_RIGHT_RAW;
static int Y_UP_RAW;
static int Y_CENTER_RAW;
static int Y_DOWN_RAW;

// EEPROM Calibration data structure
// Basically, its just a verification that the EEPROM contains valid data and that it isn't corrupted
// which, if it isn't, we can use it to load the joystick calibration values
static const uint32_t CAL_MAGIC = 0x4D424A31; // 'MBJ1'
static const int EEPROM_ADDR = 0;
struct CalibrationData
{
    uint32_t magic;
    // X axis calibration values
    int16_t xL;
    int16_t xC;
    int16_t xR;
    // Y axis calibration values
    int16_t yU;
    int16_t yC;
    int16_t yD;
    // Checksum to validate data
    uint32_t crc;
};

// CRC32 checksum
static uint32_t crc32(const void* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t* p = (const uint8_t*)data;
    while (len--)
    {
        crc ^= *p++;
        for (int k = 0; k < 8; ++k)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

static void saveCalibration(int xL, int xC, int xR, int yU, int yC, int yD)
{
    // Write calibration data to EEPROM
    CalibrationData data;
    data.magic = CAL_MAGIC;
    data.xL = (int16_t)xL;
    data.xC = (int16_t)xC;
    data.xR = (int16_t)xR;
    data.yU = (int16_t)yU;
    data.yC = (int16_t)yC;
    data.yD = (int16_t)yD;
    data.crc = crc32(&data.xL, sizeof(int16_t) * 6);
    EEPROM.put(EEPROM_ADDR, data);
}

static bool loadCalibration(int& xL, int& xC, int& xR, int& yU, int& yC, int& yD)
{
    // Load calibration data if the magic value matches
    // and if the checksum matches as well
    CalibrationData data;
    EEPROM.get(EEPROM_ADDR, data);
    if (data.magic != CAL_MAGIC)
    {
        return false;
    }

    uint32_t c = crc32(&data.xL, sizeof(int16_t) * 6);
    if (c != data.crc)
    {
        return false;
    }

    xL = data.xL;
    xC = data.xC;
    xR = data.xR;
    yU = data.yU;
    yC = data.yC;
    yD = data.yD;

    return true;
}

// Ensure ordering so that left < center < right and up < center < down
static void normalizeCalibration()
{
    // X Axis
    if (X_LEFT_RAW > X_RIGHT_RAW)
    {
        int t = X_LEFT_RAW;
        X_LEFT_RAW = X_RIGHT_RAW;
        X_RIGHT_RAW = t;
    }
    X_CENTER_RAW = constrain(X_CENTER_RAW, min(X_LEFT_RAW, X_RIGHT_RAW), max(X_LEFT_RAW, X_RIGHT_RAW));

    // Y Axis
    if (Y_UP_RAW > Y_DOWN_RAW)
    {
        int t = Y_UP_RAW;
        Y_UP_RAW = Y_DOWN_RAW;
        Y_DOWN_RAW = t;
    }
    Y_CENTER_RAW = constrain(Y_CENTER_RAW, min(Y_UP_RAW, Y_DOWN_RAW), max(Y_UP_RAW, Y_DOWN_RAW));
}

// Convert a value from one range and interpolate into another
static inline float fmap(float x, float in_min, float in_max, float out_min, float out_max)
{
    float range = in_max - in_min;
    if (fabsf(range) < 1.0f)
    {
        return (out_min + out_max) * 0.5f;
    }

    x = constrain(x, in_min, in_max);
    float t = (x - in_min) / range;
    return out_min + t * (out_max - out_min);
}

// Map raw joystick values to the [-1 - 1] HID range
static inline float mapJoystickAxis(int raw, int left_raw, int center_raw, int right_raw)
{
    if (raw <= center_raw)
    {
        return fmap(raw, left_raw, center_raw, -1.0f, 0.0f);
    }
    else
    {
        return fmap(raw, center_raw, right_raw,  0.0f, +1.0f);
    }
}

// Calculate the normalized tanh value for an input
static const float INV_TANH_K = 1.0f / tanhf(TANH_K);
static inline float tanhNorm01(float p_abs)
{
    return tanhf(TANH_K * p_abs) * INV_TANH_K;
}

// Apply deadzone and tanh curve shaping to an axis input, and clamp it to the range [-1 - 1]
// before converting the value to the joystick output range (JOY_MIN to JOY_MAX)
static int applyAxisEffects(float axis_val)
{
    float out = axis_val;
    float mag = fabsf(out);

    if (DEADZONE > 0)
    {
        // calculate the percentage of the deadzone relative to the full joystick range
        float dz = DEADZONE / (float)JOY_MAX;
        if (mag < dz)
        {
            mag = 0.0f;
        }
        else
        {
            mag = (mag - dz) / (1.0f - dz);
            mag = constrain(mag, 0.0f, 1.0f);
        }
    }

    if (ENABLE_CURVE && mag > 0.0f)
    {
        mag = tanhNorm01(mag);
    }

    out = copysignf(mag, out);
    out = constrain(out, -1.0f, +1.0f);

    float half_point = (JOY_MAX - JOY_MIN) * 0.5f;
    int joy = (int)lroundf((out * half_point) + half_point);
    return constrain(joy, JOY_MIN, JOY_MAX);
}

// Read and average N samples from a specified axis pin
static int readAveraged(int pin, int samples = NUM_SAMPLES, int delay_us=2)
{
    int read_sum = analogRead(pin);
    for (int i = 1; i < samples; i++)
    {
        delayMicroseconds(delay_us);
        read_sum += analogRead(pin);
    }
    return read_sum / samples;
}

struct Debounce
{
    bool stable = HIGH;
    bool last_read = HIGH;
    uint32_t last_change_ms = 0;
    uint32_t debounce_time = DEBOUNCE_MS;

    // Returns true when stable state changes.
    bool update(bool reading)
    {
        uint32_t now = millis();
        if (reading != last_read)
        {
            last_read = reading;
            last_change_ms = now;
        }
        if ((now - last_change_ms) >= debounce_time && stable != reading)
        {
            stable = reading;
            return true;
        }
        return false;
    }
};


static void blinkLED(uint8_t n, uint16_t on_ms=120, uint16_t off_ms=120)
{
    for (uint8_t i = 0; i < n; i++)
    {
        TXLED1;
        delay(on_ms);
        TXLED0;
        delay(off_ms);
    }
}

// Calibration initial modes
static Mode mode = MODE_NORMAL;
static CalibrationStep calibration_step = CAL_NONE;

static void abortCalibration()
{
    // Restore previous calibration from EEPROM (or keep defaults if EEPROM was empty)
    if (!loadCalibration(X_LEFT_RAW, 
                        X_CENTER_RAW, 
                        X_RIGHT_RAW,
                        Y_UP_RAW, 
                        Y_CENTER_RAW, 
                        Y_DOWN_RAW))
    {
        X_LEFT_RAW = DEFAULT_X_LEFT;
        X_CENTER_RAW = DEFAULT_X_CENTER;
        X_RIGHT_RAW = DEFAULT_X_RIGHT;
        Y_UP_RAW = DEFAULT_Y_UP;
        Y_CENTER_RAW = DEFAULT_Y_CENTER;
        Y_DOWN_RAW = DEFAULT_Y_DOWN;
    }
    normalizeCalibration();
    if (SERIAL_OUTPUT)
    {
        Serial.println("Calibration aborted. Previous calibration restored");
    }
    blinkLED(2, 200, 200);
    TXLED0;
    calibration_step = CAL_DONE;
    mode = MODE_NORMAL;
}

static void startCalibration()
{
    mode = MODE_CALIBRATING;
    calibration_step = CAL_CENTER;

    // Freeze HID at center while calibrating
    Joystick.setXAxis((JOY_MAX + JOY_MIN) / 2);
    Joystick.setYAxis((JOY_MAX + JOY_MIN) / 2);

    // Also release the multitap HID button to avoid stuck press
    Joystick.setButton(MULTITAP_BUTTON_INDEX, 0);
    Joystick.sendState();

    if (SERIAL_OUTPUT)
    {
        Serial.println();
        Serial.println("ENTERING CALIBRATION MODE");
        Serial.print("hold ");
        Serial.print(MULTITAP_BUTTON_NAME);
        Serial.print(" for ");
        Serial.print(MULTITAP_HOLD_SEC);
        Serial.println(" seconds at any step to abort");
        Serial.print("Step 1/5: Let go of button, then press ");
        Serial.print(MULTITAP_BUTTON_NAME);
        Serial.println(" to capture CENTER");
    }
    blinkLED(3, 90, 90);
}

static void finishCalibration()
{
    normalizeCalibration();
    saveCalibration(X_LEFT_RAW, 
                    X_CENTER_RAW, 
                    X_RIGHT_RAW,
                    Y_UP_RAW, 
                    Y_CENTER_RAW, 
                    Y_DOWN_RAW);
    if (SERIAL_OUTPUT)
    {
        Serial.println("Saved calibration to EEPROM");
        Serial.print("X: L=");
        Serial.print(X_LEFT_RAW);
        Serial.print("  C=");
        Serial.print(X_CENTER_RAW);
        Serial.print("  R=");
        Serial.println(X_RIGHT_RAW);
        Serial.print("Y: U=");
        Serial.print(Y_UP_RAW);
        Serial.print("  C=");
        Serial.print(Y_CENTER_RAW);
        Serial.print("  D=");
        Serial.println(Y_DOWN_RAW);
        Serial.println("EXITING CALIBRATION MODE");
    }
    blinkLED(5, 80, 80);
    calibration_step = CAL_DONE;
    mode = MODE_NORMAL;
}

// Declaration for each of the buttons
static Debounce start_button;
static Debounce hotkey_button;
static Debounce service_button;
static Debounce test_button;
static Debounce coin_button;

// Pointer to the button used for multitap calibration button
static Debounce* multitap_button = nullptr;

// Coin pulse state so we can emit a static pulse length when the button triggers
static bool coin_pulse_active = false;
static uint32_t coin_pulse_start_ms = 0;
static uint32_t coin_last_accept_ms = 0;

// Multi-tap tracking
static uint8_t tap_count = 0;
static uint32_t last_tap_ms = 0;
static uint32_t hold_start_ms = 0;
static bool waiting_hold = false;

// Arduino initialization on power on
void setup()
{
    if (SERIAL_OUTPUT)
    {
        Serial.begin(115200);
        delay(200);
        Serial.println("Monkey Ball Joystick - Written by ShellHg");
    }

    TXLED0;
    RXLED0;

    // Buttons with internal pullup (active LOW)
    pinMode(START_PIN, INPUT_PULLUP);
    pinMode(HOTKEY_PIN, INPUT_PULLUP);
    pinMode(SERVICE_PIN, INPUT_PULLUP);
    pinMode(TEST_PIN, INPUT_PULLUP);
    pinMode(COIN_PIN, INPUT_PULLUP);

    // Set the pointer for the multitap calibration button
    if (MULTITAP_BUTTON_PIN == START_PIN)
    {
        multitap_button = &start_button;
    }
    else if (MULTITAP_BUTTON_PIN == HOTKEY_PIN)
    {
        multitap_button = &hotkey_button;
    }
    else if (MULTITAP_BUTTON_PIN == TEST_PIN)
    {
        multitap_button = &test_button;
    }
    else
    {
        // MULTITAP_BUTTON_PIN == SERVICE_PIN fallback
        multitap_button = &service_button;
    }

    // Set proper debounce time for the coin mechanism
    if (REAL_COIN_SLOT)
    {
        coin_button.debounce_time = COIN_DEBOUNCE_MS;
    }

    // Joystick axis range and manual send mode
    Joystick.setXAxisRange(JOY_MIN, JOY_MAX);
    Joystick.setYAxisRange(JOY_MIN, JOY_MAX);
    Joystick.begin(false);

    // Load calibration from EEPROM
    if (!loadCalibration(X_LEFT_RAW, 
                        X_CENTER_RAW, 
                        X_RIGHT_RAW, 
                        Y_UP_RAW, 
                        Y_CENTER_RAW, 
                        Y_DOWN_RAW))
    {
        if (SERIAL_OUTPUT)
        {
            Serial.println("EEPROM empty... writing default values for joystick calibration");
        }
        X_LEFT_RAW   = DEFAULT_X_LEFT;
        X_CENTER_RAW = DEFAULT_X_CENTER;
        X_RIGHT_RAW  = DEFAULT_X_RIGHT;
        Y_UP_RAW     = DEFAULT_Y_UP;
        Y_CENTER_RAW = DEFAULT_Y_CENTER;
        Y_DOWN_RAW   = DEFAULT_Y_DOWN;
        normalizeCalibration();
    }
    else
    {
        if (SERIAL_OUTPUT)
        {
            Serial.println("Loaded joystick calibration from EEPROM");
        }
        normalizeCalibration();
    }

    if (SERIAL_OUTPUT)
    {
        Serial.print("X: L=");
        Serial.print(X_LEFT_RAW);
        Serial.print("  C=");
        Serial.print(X_CENTER_RAW);
        Serial.print("  R=");
        Serial.print(X_RIGHT_RAW);
        Serial.print(" | Y: U=");
        Serial.print(Y_UP_RAW);
        Serial.print("  C=");
        Serial.print(Y_CENTER_RAW);
        Serial.print("  D=");
        Serial.println(Y_DOWN_RAW);
        Serial.print("To recalibrate, press ");
        Serial.print(MULTITAP_BUTTON_NAME);
        Serial.print(" ");
        Serial.print(MULTITAP_COUNT);
        Serial.print(" times, holding the last for ");
        Serial.print(MULTITAP_HOLD_SEC);
        Serial.println(" seconds");
    }
}

// Check for multitap calibration entry (NORMAL mode only)
static void checkForCalibration(uint32_t now, bool mt_pressed)
{
    if (mode != MODE_NORMAL)
    {
        return;
    }

    if (mt_pressed)
    {
        // Reset tap sequence if gap too large
        if (tap_count > 0 && (now - last_tap_ms) > MULTITAP_MAX_GAP_MS)
        {
            tap_count = 0;
            waiting_hold = false;
        }
        tap_count++;
        last_tap_ms = now;

        if (tap_count == MULTITAP_COUNT)
        {
            // Start measuring the hold on the last press
            waiting_hold = true;
            hold_start_ms = now;
            if (SERIAL_OUTPUT)
            {
                Serial.print(MULTITAP_BUTTON_NAME);
                Serial.print(" press #");
                Serial.print(MULTITAP_COUNT);
                Serial.print(" detected. Hold for ");
                Serial.print(MULTITAP_HOLD_SEC);
                Serial.println(" seconds to enter calibration mode...");
            }
        }
    }

    if (waiting_hold)
    {
        if (multitap_button->stable == LOW)
        {
            if ((now - hold_start_ms) >= MULTITAP_HOLD_SEC * 1000)
            {
                // Enter calibration mode
                waiting_hold = false;
                tap_count = 0;
                startCalibration();
            }
        }
        else
        {
            // Released before the required hold, so cancel the calibration entry
            waiting_hold = false;
            tap_count = 0;
            if (SERIAL_OUTPUT)
            {
                Serial.println("Hold too short. Calibration cancelled.");
            }
        }
    }

    // Reset tap count if the time gap between presses is exceeded
    if (!waiting_hold && tap_count > 0 && (now - last_tap_ms) > MULTITAP_MAX_GAP_MS)
    {
        tap_count = 0;
    }
}

// Process calibration steps, returning true if we are in calibration mode
static bool calibrationProcessing(uint32_t now, bool mt_pressed)
{
    if (mode != MODE_CALIBRATING)
    {
        return false;
    }

    // Heartbeat LED blink while in calibration on TX LED
    static uint32_t heartbeat_ms = 0;
    static bool tx_led_state = false;
    RXLED0;
    if (now - heartbeat_ms > 400)
    {
        heartbeat_ms = now;
        tx_led_state = !tx_led_state;
        if (tx_led_state)
        {
            TXLED1;
        }
        else
        {
            TXLED0;
        }
    }

    // Abort: hold the multitap button for MULTITAP_HOLD_SEC seconds during any calibration step
    static uint32_t calibration_hold_start = 0;
    if (mt_pressed)
    {
        calibration_hold_start = now;
    }
    if (multitap_button->stable == LOW && calibration_hold_start > 0)
    {
        if ((now - calibration_hold_start) >= MULTITAP_HOLD_SEC * 1000)
        {
            calibration_hold_start = 0;
            abortCalibration();
            return true;
        }
    }
    else if (multitap_button->stable == HIGH)
    {
        calibration_hold_start = 0;
    }

    // Process current calibration step
    if (mt_pressed)
    {
        switch (calibration_step)
        {
            case CAL_CENTER:
            {
                X_CENTER_RAW = readAveraged(PIN_X, 32, 5);
                Y_CENTER_RAW = readAveraged(PIN_Y, 32, 5);
                if (SERIAL_OUTPUT)
                {
                    Serial.print("Captured CENTER: Xc=");
                    Serial.print(X_CENTER_RAW);
                    Serial.print("  Yc=");
                    Serial.println(Y_CENTER_RAW);
                    Serial.println("Step 2/5: Push UP fully, then press the button.");
                }
                blinkLED(1, 50, 50);
                calibration_step = CAL_Y_UP;
                break;
            }
            case CAL_Y_UP:
            {
                Y_UP_RAW = readAveraged(PIN_Y, 32, 5);
                if (SERIAL_OUTPUT)
                {
                    Serial.print("Captured Y UP: Yu=");
                    Serial.println(Y_UP_RAW);
                    Serial.println("Step 3/5: Push DOWN fully, then press the button.");
                }
                blinkLED(1, 50, 50);
                calibration_step = CAL_Y_DOWN;
                break;
            }
            case CAL_Y_DOWN:
            {
                Y_DOWN_RAW = readAveraged(PIN_Y, 32, 5);
                if (SERIAL_OUTPUT)
                {
                    Serial.print("Captured Y DOWN: Yd=");
                    Serial.println(Y_DOWN_RAW);
                    Serial.println("Step 4/5: Push RIGHT fully, then press the button.");
                }
                blinkLED(1, 50, 50);
                calibration_step = CAL_X_RIGHT;
                break;
            }
            case CAL_X_RIGHT:
            {
                X_RIGHT_RAW = readAveraged(PIN_X, 32, 5);
                if (SERIAL_OUTPUT)
                {
                    Serial.print("Captured X RIGHT: Xr=");
                    Serial.println(X_RIGHT_RAW);
                    Serial.println("Step 5/5: Push LEFT fully, then press the button.");
                }
                blinkLED(1, 50, 50);
                calibration_step = CAL_X_LEFT;
                break;
            }
            case CAL_X_LEFT:
            {
                X_LEFT_RAW = readAveraged(PIN_X, 32, 5);
                if (SERIAL_OUTPUT)
                {
                    Serial.print("Captured X LEFT: Xl=");
                    Serial.println(X_LEFT_RAW);
                }
                finishCalibration();
                break;
            }
            default:
            {
                // Shouldn't happen, but just in case, abort the calibration
                if (SERIAL_OUTPUT)
                {
                    Serial.println("Invalid calibration step. Aborting.");
                }
                abortCalibration();
            }
        }
    }

    // Keep HID centered and multitap button released while calibrating
    Joystick.setXAxis((JOY_MAX + JOY_MIN) / 2);
    Joystick.setYAxis((JOY_MAX + JOY_MIN) / 2);
    Joystick.setButton(MULTITAP_BUTTON_INDEX, 0);
    static uint32_t last_calibration_send_ms = 0;
    if ((now - last_calibration_send_ms) >= HID_SEND_INTERVAL_MS)
    {
        last_calibration_send_ms = now;
        Joystick.sendState();
    }
    return true;
}

// Main processing loop
void loop()
{
    // There needs to be a rate limit in order to prevent
    // the USB host from being overwhelmed with HID packets
    // and causing button input drops
    static uint32_t last_loop_us = 0;
    uint32_t now_us = micros();
    uint32_t now = millis();
    if ((now_us - last_loop_us) < LOOP_INTERVAL_US)
    {
        return;
    }
    last_loop_us = now_us;

    // Set initial values to the center point of our ADC range
    static float ema_x = 512.0f;
    static float ema_y = 512.0f;

    // Read axes and apply processing effects
    // Use Exponential Moving Average to smooth out 
    // readings over time after applying the basic effects
    int read_x = readAveraged(PIN_X);
    int read_y = readAveraged(PIN_Y);
    if (ENABLE_EMA)
    {
        ema_x = EMA_ALPHA * read_x + (1.0f - EMA_ALPHA) * ema_x;
        ema_y = EMA_ALPHA * read_y + (1.0f - EMA_ALPHA) * ema_y;
        read_x = (int)lroundf(ema_x);
        read_y = (int)lroundf(ema_y);
    }

    // Update all button debounces
    bool start_changed   = start_button.update(digitalRead(START_PIN));
    bool hotkey_changed  = hotkey_button.update(digitalRead(HOTKEY_PIN));
    bool test_changed    = test_button.update(digitalRead(TEST_PIN));
    bool service_changed = service_button.update(digitalRead(SERVICE_PIN));

    // Track the multitap calibration button
    bool mt_changed = false;
    if      (MULTITAP_BUTTON_PIN == START_PIN)
    {
        mt_changed = start_changed;
    }
    else if (MULTITAP_BUTTON_PIN == HOTKEY_PIN)
    {
        mt_changed = hotkey_changed;
    }
    else if (MULTITAP_BUTTON_PIN == TEST_PIN)
    {
        mt_changed = test_changed;
    }
    else
    {
        // MULTITAP_BUTTON_PIN == SERVICE_PIN fallback
        mt_changed = service_changed;
    }
    bool mt_pressed = mt_changed && (multitap_button->stable == LOW);

    // Process button changes excluding whichever button is the Multitap button
    // since that is handled separately for calibration mode
    if (start_changed && START_PIN != MULTITAP_BUTTON_PIN)
    {
        Joystick.setButton(START_INDEX, start_button.stable == LOW ? 1 : 0);
    }
    if (hotkey_changed && HOTKEY_PIN != MULTITAP_BUTTON_PIN)
    {
        Joystick.setButton(HOTKEY_INDEX, hotkey_button.stable == LOW ? 1 : 0);
    }
    if (test_changed && TEST_PIN != MULTITAP_BUTTON_PIN)
    {
        Joystick.setButton(TEST_INDEX, test_button.stable == LOW ? 1 : 0);
    }
    if (service_changed && SERVICE_PIN != MULTITAP_BUTTON_PIN)
    {
        Joystick.setButton(SERVICE_INDEX, service_button.stable == LOW ? 1 : 0);
    }

    // Coin button handling
    if (REAL_COIN_SLOT)
    {
        // Switch is Normally Closed, so idle is LOW, activated is HIGH
        // Trigger a fixed-length pulse on each coin insert with a cooldown
        coin_button.update(digitalRead(COIN_PIN));
        if (coin_button.stable == HIGH && !coin_pulse_active
            && (now - coin_last_accept_ms) >= COIN_COOLDOWN_MS)
        {
            coin_pulse_active = true;
            coin_pulse_start_ms = now;
            coin_last_accept_ms = now;
            Joystick.setButton(COIN_INDEX, 1);
            if (SERIAL_OUTPUT)
            {
                Serial.println("Coin accepted");
            }
        }
        if (coin_pulse_active && (now - coin_pulse_start_ms) >= COIN_PULSE_MS)
        {
            coin_pulse_active = false;
            Joystick.setButton(COIN_INDEX, 0);
        }
    }
    else
    {
        if (coin_button.update(digitalRead(COIN_PIN)))
        {
            Joystick.setButton(COIN_INDEX, coin_button.stable == LOW ? 1 : 0);
        }
    }

    checkForCalibration(now, mt_pressed);

    if (calibrationProcessing(now, mt_pressed))
    {
        // Skip normal processing while in calibration mode
        return;
    }

    // Map raw input values to axis ranges using current calibration
    float mapped_x = mapJoystickAxis(read_x, X_LEFT_RAW, X_CENTER_RAW, X_RIGHT_RAW);
    float mapped_y = mapJoystickAxis(read_y, Y_UP_RAW, Y_CENTER_RAW, Y_DOWN_RAW);

    if (INVERT_X)
    {
        mapped_x = -mapped_x;
    }
    if (INVERT_Y)
    {
        mapped_y = -mapped_y;
    }

    // Process the axis values
    int output_x = applyAxisEffects(mapped_x);
    int output_y = applyAxisEffects(mapped_y);

    // Mirror debounced multitap button to HID report when not calibrating
    if (mt_changed)
    {
        Joystick.setButton(MULTITAP_BUTTON_INDEX, multitap_button->stable == LOW ? 1 : 0);
    }

    // RX LED lights when joystick is near the extremes or any button is held
    bool coin_active = REAL_COIN_SLOT ? (coin_button.stable == HIGH) : (coin_button.stable == LOW);
    bool any_button_held = (start_button.stable == LOW || hotkey_button.stable == LOW
                            || test_button.stable == LOW || service_button.stable == LOW
                            || coin_active);
    bool joystick_active = (output_x < 100 || output_x > (JOY_MAX - 100)
                            || output_y < 100 || output_y > (JOY_MAX - 100));
    if (joystick_active || any_button_held)
    {
        RXLED1;
    }
    else
    {
        RXLED0;
    }

    // Send HID report with all changes during this loop
    Joystick.setXAxis(output_x);
    Joystick.setYAxis(output_y);
    static uint32_t last_send_ms = 0;
    if ((now - last_send_ms) >= HID_SEND_INTERVAL_MS)
    {
        last_send_ms = now;
        Joystick.sendState();
        TXLED0;
    }
}
