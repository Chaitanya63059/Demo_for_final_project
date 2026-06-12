# ESP32 Smart Home Wokwi Simulation

This is a small smart-home safety project made for learning Embedded C with an
ESP32. It reads several sensors, shows the values on an LCD, and reacts using a
buzzer, relay, servo and NeoPixel ring.

The main logic is in `main.c`. The small `hardware.cpp` file is only needed
because the Arduino libraries used by the ESP32 are written in C++.

## Components

The controller is an ESP32 DevKit V4.

Inputs used in the circuit:

- DHT22 temperature and humidity sensor
- MQ2 gas sensor
- LDR light sensor
- Flame sensor
- PIR motion sensor
- HC-SR04 distance sensor
- Potentiometer used as a water-leak input
- Panic pushbutton
- DS1307 real-time clock

Outputs used are a 20x4 LCD, NeoPixel ring, buzzer, servo motor and relay.

## Sensors And Thresholds

**DHT22:** Connected to GPIO 15. It sends temperature and humidity using a
single digital data line. Humidity is displayed only. The temperature alarm
starts above `45 C`.

**MQ2 gas sensor:** AO is connected to GPIO 39 and DO to GPIO 34. AO gives a
relative gas value, while DO becomes LOW after the module threshold is crossed.
The alarm starts when AO is above `3500` or DO is LOW.

**LDR:** Its analog output is connected to GPIO 36. A larger value represents a
darker environment in this simulation. Above `2500`, the NeoPixel ring turns
blue, but the buzzer does not sound.

**Flame sensor:** DO is connected to GPIO 35. The output is active-low, so LOW
means flame is detected. This condition starts the alarm.

**PIR sensor:** OUT is connected to GPIO 25. HIGH means motion has been
detected. Motion changes the ring to orange without starting the alarm.

**HC-SR04:** TRIG is connected to GPIO 26 and ECHO to GPIO 27. The ESP32
measures the echo pulse to calculate distance. Below `20 cm`, the servo opens.

**Water input:** A potentiometer on GPIO 33 is used to imitate a water-leak
sensor. A reading above `3000` starts the alarm.

**Panic button:** Connected to GPIO 32 with the ESP32 internal pull-up. Pressing
the button makes the input LOW and starts the alarm.

**DS1307 RTC:** Shares the I2C bus with the LCD. It provides the time shown on
the display and does not have an alarm threshold.

The alarm is disabled during the first `5 seconds` after startup. When an alarm
is active, the relay turns off, the servo opens, the buzzer plays a 1200 Hz
tone, and the ring turns red.

## Communication And Pins

- **UART:** Serial Monitor at 115200 baud.
- **I2C:** LCD and DS1307 use GPIO 21 for SDA and GPIO 22 for SCL.
- **ADC:** MQ2 AO uses GPIO 39, LDR uses GPIO 36, and water input uses GPIO 33.
- **Digital inputs:** MQ2 DO uses GPIO 34, flame GPIO 35, PIR GPIO 25, and panic GPIO 32.
- **Pulse timing:** HC-SR04 uses GPIO 26 for TRIG and GPIO 27 for ECHO.
- **NeoPixel data:** GPIO 2.
- **Servo PWM:** GPIO 13.
- **Buzzer output:** GPIO 4.
- **Relay output:** GPIO 14.
- **Wi-Fi:** Uses the ESP32 radio and no external GPIO.
- **MQTT and HTTP:** Run over the Wi-Fi connection.

## What Happens At Startup

1. The ESP32 configures all input and output pins.
2. The relay starts on and the buzzer stays silent.
3. The LCD, RTC, DHT22, NeoPixel ring and servo are initialized.
4. The ESP32 connects to the Wokwi Wi-Fi network.
5. The LCD asks the user to wait five seconds.
6. Sensor readings are checked every two seconds.

## Running The Project

Install VS Code, Git, PlatformIO IDE and the Wokwi Simulator extension first.
A Wokwi license may be needed for the VS Code simulator.

To use this repository:

```powershell
git clone <repository-url>
cd demo_for_final_project
code .
```

PlatformIO installs the required libraries automatically from `platformio.ini`.
These include LiquidCrystal_I2C, the Adafruit DHT and Unified Sensor libraries,
Adafruit NeoPixel, PubSubClient, ESP32Servo and RTClib.

Build the firmware using the PlatformIO check-mark button or:

```powershell
pio run
```

After the build finishes:

1. Open the VS Code Command Palette.
2. Run `Wokwi: Start Simulator`.
3. Open `diagram.json` to view the circuit.
4. Change the sensor values to test each condition.
5. Rebuild the project after changing the code.

### Setting Up the PlatformIO Project from Scratch
If you'd rather build the project manually instead of cloning:

1. Install VS Code.
2. Open Extensions (Ctrl+Shift+X) and install PlatformIO IDE.
3. Open PlatformIO Home from the sidebar icon.
4. Click New Project.
5. Name it something like ESP32-Smart-Home-Wokwi.
6. Select Espressif ESP32 Dev Module as the board and Arduino as the framework.
7. Create the project and let PlatformIO finish setting up.
8. Replace the generated platformio.ini with the one from this repo.
9. Place main.c, hardware.cpp, diagram.json, and wokwi.toml in the project root.
10. Hit the PlatformIO check-mark to build.

## Real Hardware Note

This circuit is designed for simulation. A physical ESP32 uses 3.3 V GPIO.
Some 5 V modules, especially the HC-SR04 ECHO pin, need a voltage divider or
level shifter before being connected to real hardware.

## Links

- VS Code: https://code.visualstudio.com/download
- Git: https://git-scm.com/downloads
- PlatformIO IDE: https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide
- Wokwi extension: https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode
- Wokwi setup guide: https://docs.wokwi.com/vscode/getting-started
- Wokwi project configuration: https://docs.wokwi.com/vscode/project-config
- Wokwi diagram format: https://docs.wokwi.com/diagram-format
- Wokwi pricing: https://wokwi.com/pricing
- LiquidCrystal_I2C: https://github.com/marcoschwartz/LiquidCrystal_I2C
- Adafruit DHT: https://github.com/adafruit/DHT-sensor-library
- Adafruit Unified Sensor: https://github.com/adafruit/Adafruit_Sensor
- Adafruit NeoPixel: https://github.com/adafruit/Adafruit_NeoPixel
- PubSubClient: https://github.com/knolleary/pubsubclient
- ESP32Servo: https://github.com/madhephaestus/ESP32Servo
- RTClib: https://github.com/adafruit/RTClib
