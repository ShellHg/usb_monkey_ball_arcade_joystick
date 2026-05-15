# USB Monkey Ball Arcade Joystick

This is a solution for bypassing the need for a dedicated amplifier and signal processor for the Monkey Ball arcade joystick, combining with buttons and presenting itself as a USB Joystick to play the game.
I wrote this project in order to solve my personal Monkey Ball arcade cabinet build which you can check out on the Arcade-Projects forum: https://www.arcade-projects.com/threads/my-monkey-ball-arcade-cabinet-build.37291/#post-495277

## My Hardware

This is the hardware that this program was written and intended for. I don't really know how portable it is to other Arduino platforms, but at the very least, here is what I used:
- Arduino Pro Micro (5V version)
- Sanwa JLK-GF2-EV analog 2-axis joystick
- 5 arcade buttons (active LOW with internal pullup)
- a Coin Mechanism (normally closed switch)

### Pin Map

Refer to the table below to understand how each component in this build is connected to the Arduino Pro Micro board pins:

| Button Function    | Arduino Pin | HID Button |
|--------------------|-------------|------------|
| START              | Pin 2       | HID #1     |
| HOTKEY             | Pin 3       | HID #2     |
| SERVICE (X Button) | Pin 4       | HID #3     |
| TEST (Y Button)    | Pin 5       | HID #4     |
| SELECT (Coin Slot) | Pin 6       | HID #5     |
| Joystick X Axis    | A0          | Left Axis X|
| Joystick Y Axis    | A1          | Left Axis Y|
| Joystick X VCC     | VCC (5V)    | -          |
| Joystick Y VCC     | VCC (5V)    | -          |

I am currently working on a schematic to visually show how everything is hooked up, but that is not my expertise, so it may take a little bit... haha

## The Analog Signal Processing Pipeline

1. **Analog to Digital Conversion (ADC) Oversampling** - Multiple analog reads are averaged together per loop to reduce noise. Controlled by `ENABLE_SMOOTHING` (default: 4 samples).

2. **Exponential Moving Average (EMA)** - Smooths the averaged readings over time. Higher `EMA_ALPHA` values (closer to 1) are more responsive but noisier. Lower values are smoother but add input lag. Controlled by `ENABLE_EMA`.

3. **Asymmetric Mapping** - The raw ADC value is mapped to a signed [-1 to 1] range using the calibrated left/center/right and up/center/down positions.

4. **Deadzone** - A circular deadzone around the center. Any input within the deadzone radius is treated as zero. Values outside the deadzone are rescaled so movement starts smoothly from the edge of the deadzone.

5. **Tanh Response Curve** - Applies a normalized tanh curve to the magnitude, giving finer control near center and faster response near the edges. The `TANH_K` value controls how tight the curve is around center. Controlled by `ENABLE_CURVE`.

6. **HID Output** - The processed value is scaled to the [0 to 1023] HID axis range and sent to the host via USB at a rate-limited interval to prevent packet drops. The polling rate of this joystick is set by default to 500Hz, which is already 5 times the polling rate of the original joystick on Naomi hardware.

### Button Debouncing

All buttons use a debounce filter to prevent false triggers from electrical noise. The coin mechanism by default uses a longer debounce time (25ms vs 1ms).

### Coin Mechanism

If `REAL_COIN_SLOT` is true, the coin button uses special handling for a normally closed (NC) coin switch. Instead of directly mirroring the switch state, it emits a fixed-length pulse on each coin insert with a cooldown period to prevent double-counts. Set `REAL_COIN_SLOT` to false if you are using a regular button.

## Joystick Calibration

Calibration values storing a variety of data with the physical hardware X and Y ranges of the joystick are saved to EEPROM and persist across power cycles. The EEPROM data is validated with a magic value and a CRC32 checksum. If the EEPROM data is missing or corrupted, the firmware falls back to the default calibration values in `config.h`.

### Entering Calibration Mode

Press the calibration button (`MULTITAP_BUTTON`, default: HOTKEY) 8 times in a row, holding the last press for 3 seconds. Each press must be within 1200ms of the previous one. The TX LED will blink 3 times to confirm entry.

### Calibration Steps

1. Release the joystick to center, then press the button
2. Push the joystick fully UP, then press the button
3. Push the joystick fully DOWN, then press the button
4. Push the joystick fully RIGHT, then press the button
5. Push the joystick fully LEFT, then press the button

The TX LED blinks once after each step. After the last step, calibration is saved to EEPROM and the TX LED blinks 5 times.

### Aborting Calibration

Hold the calibration button for 3 seconds at any step. The previous calibration will be restored from EEPROM. The TX LED blinks 2 times to confirm abort.

## Configuration

All configurable values are in `src/config.h`. Edit this file and re-flash the Pro Micro to apply changes.

### Processing Flags

| Flag               | Default | Description                                      |
|--------------------|---------|--------------------------------------------------|
| `SERIAL_OUTPUT`    | false   | Print debug logs to USB serial output (115200 baud)     |
| `ENABLE_CURVE`     | true    | Apply tanh response curve to joystick axes       |
| `ENABLE_SMOOTHING` | true    | Average multiple ADC samples per loop            |
| `ENABLE_EMA`       | true    | Apply exponential moving average to axis readings|
| `INVERT_X`         | false   | Invert the X axis direction                      |
| `INVERT_Y`         | false   | Invert the Y axis direction                      |
| `REAL_COIN_SLOT`   | true    | Use coin mechanism pulse logic instead of simple button |

### Tunable Values

| Value       | Default | Description                                              |
|-------------|---------|----------------------------------------------------------|
| `DEADZONE`  | 70      | Deadzone radius in digital units. Range is [0 to 1023]   |
| `TANH_K`    | 1.6     | Tanh curve tightness (higher = tighter center)           |
| `EMA_ALPHA` | 0.9     | EMA responsiveness. Range is [0 to 1], higher means more responsive |

### Calibration Button

The button used for entering calibration mode and advancing through steps is configurable. Change all three of these together:

```cpp
const int    MULTITAP_BUTTON_PIN   = HOTKEY_PIN;    // which pin to read
const int    MULTITAP_BUTTON_INDEX = HOTKEY_INDEX;   // which HID button index
const char*  MULTITAP_BUTTON_NAME  = "HOTKEY";       // name for serial output
```

For example, to use the START button instead:

```cpp
const int    MULTITAP_BUTTON_PIN   = START_PIN;
const int    MULTITAP_BUTTON_INDEX = START_INDEX;
const char*  MULTITAP_BUTTON_NAME  = "START";
```

### Calibration Entry Timing

| Value                 | Default | Description                                       |
|-----------------------|---------|---------------------------------------------------|
| `MULTITAP_COUNT`      | 8       | Number of button presses to trigger calibration   |
| `MULTITAP_MAX_GAP_MS` | 1200   | Max time allowed between consecutive presses |
| `MULTITAP_HOLD_SEC`   | 3       | How long to hold the last press         |

### Coin Mechanism Timing

| Value              | Default | Description                                |
|--------------------|---------|--------------------------------------------|
| `COIN_DEBOUNCE_MS` | 25      | Debounce time for the coin switch          |
| `COIN_PULSE_MS`    | 100     | Duration of the HID button pulse per coin  |
| `COIN_COOLDOWN_MS` | 100     | Minimum time between accepted coin inserts |

### Pin Assignments

If your wiring differs from the default, update the pin and HID index pairs in `config.h`. Each button has a `_PIN` (Arduino pin number) and `_INDEX` (HID button number). You can use this to change the wiring for different Arduino boards.

### Default Calibration Values

The `DEFAULT_X_*` and `DEFAULT_Y_*` values are fallback calibration values used when EEPROM is empty or corrupted. These should be rough measurements from your specific joystick. By default I set them to my own joystick's rough values when I was first testing with it.

## Building

This requires VSCode with the PlatformIO extension, or the Arduino IDE with the Arduino Pro Micro board package and the ArduinoJoystickLibrary installed.

I use the PlatformIO extension with VSCode which handles all of that for me and is automated. The `platformio.ini` file is already configured for the Arduino Pro Micro with all required libraries and build flags.

I will have to update this file in the future to explain comprehensive steps for building. Please for now look up on Google or somewhere else how to build and upload when PlatformIO is not present
