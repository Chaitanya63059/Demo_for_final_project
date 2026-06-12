#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
/* ---------------- ESP32 PIN NAMES ---------------- */
#define DHT_PIN 15
#define LDR_PIN 36
#define GAS_AO_PIN 39
#define GAS_DO_PIN 34
#define FLAME_PIN 35
#define PIR_PIN 25
#define TRIG_PIN 26
#define ECHO_PIN 27
#define WATER_PIN 33
#define PANIC_PIN 32
#define BUZZER_PIN 4
#define SERVO_PIN 13
#define RELAY_PIN 14
/* Simple names used instead of Arduino C++ constants. */
#define HW_INPUT 0
#define HW_OUTPUT 1
#define HW_INPUT_PULLUP 2
#define HW_LOW 0
#define HW_HIGH 1
/*
 * Functions supplied by hardware.cpp.
 * These declarations let this file remain normal C code.
 */
void hw_serial_begin(int baud);
void hw_serial_println(const char* text);
void hw_serial_status(float temp, int gas, bool alarm);
void hw_pin_mode(int pin, int mode);
void hw_digital_write(int pin, int value);
int hw_digital_read(int pin);
int hw_analog_read(int pin);
void hw_delay_ms(unsigned long ms);
void hw_delay_us(unsigned int us);
unsigned long hw_millis(void);
unsigned long hw_pulse_high(int pin, unsigned long timeout_us);
void hw_tone(int pin, int frequency);
void hw_no_tone(int pin);

void hw_i2c_begin(void);
void hw_lcd_begin(void);
void hw_lcd_print(int row, const char* text);
void hw_rtc_begin(void);
int hw_rtc_hour(void);
int hw_rtc_minute(void);

void hw_dht_begin(void);
float hw_read_temperature(void);
float hw_read_humidity(void);

void hw_ring_begin(void);
void hw_ring_color(uint8_t red, uint8_t green, uint8_t blue);
void hw_servo_begin(int pin);
void hw_servo_angle(int angle);

void hw_wifi_connect(void);
void hw_mqtt_begin(void);
void hw_mqtt_connect(void);
void hw_mqtt_loop(void);
bool hw_mqtt_connected(void);
void hw_mqtt_publish(const char* topic, const char* value);
void hw_http_alert(const char* reason);
/* ---------------- SIMPLE THRESHOLDS ---------------- */
#define TEMP_LIMIT_C 45.0f
#define GAS_LIMIT 3500
#define WATER_LIMIT 3000
#define DARK_LIMIT 2500
#define NEAR_DOOR_CM 20.0f
#define SAFE_START_MS 5000UL
static bool alarm_active = false;
static bool alert_sent = false;
static float read_distance_cm(void) {
  /* HC-SR04: send a 10 us pulse on TRIG (GPIO 26). */
  hw_digital_write(TRIG_PIN, HW_LOW);
  hw_delay_us(2);
  hw_digital_write(TRIG_PIN, HW_HIGH);
  hw_delay_us(10);
  hw_digital_write(TRIG_PIN, HW_LOW);
  /* HC-SR04: measure the ECHO pulse on GPIO 27. */
  unsigned long duration = hw_pulse_high(ECHO_PIN, 30000);
  if (duration == 0) return 400.0f;
  return (float)duration * 0.0343f / 2.0f;
}
static void publish_value(const char* topic, int value) {
  char text[16];
  snprintf(text, sizeof(text), "%d", value);
  hw_mqtt_publish(topic, text);
}
static void publish_float(const char* topic, float value) {
  char text[16];
  snprintf(text, sizeof(text), "%.1f", value);
  hw_mqtt_publish(topic, text);
}
void start_system(void) {
  /* UART0 protocol: USB Serial Monitor, 115200 bits per second. */
  hw_serial_begin(115200);
  hw_serial_println("Smart Home Teaching Demo - C version");
  /* Configure each ESP32 line according to the connected sensor/output. */
  hw_pin_mode(GAS_DO_PIN, HW_INPUT);
  hw_pin_mode(FLAME_PIN, HW_INPUT);
  hw_pin_mode(PIR_PIN, HW_INPUT);
  hw_pin_mode(PANIC_PIN, HW_INPUT_PULLUP);
  hw_pin_mode(TRIG_PIN, HW_OUTPUT);
  hw_pin_mode(ECHO_PIN, HW_INPUT);
  hw_pin_mode(BUZZER_PIN, HW_OUTPUT);
  hw_pin_mode(RELAY_PIN, HW_OUTPUT);
  /* Start in a safe and silent condition. */
  hw_digital_write(RELAY_PIN, HW_HIGH);
  hw_no_tone(BUZZER_PIN);
  /* I2C: LCD2004 and DS1307 share GPIO 21 SDA and GPIO 22 SCL. */
  hw_i2c_begin();
  hw_lcd_begin();
  hw_rtc_begin();
  /* Start the DHT22, NeoPixel ring and servo output. */
  hw_dht_begin();
  hw_ring_begin();
  hw_servo_begin(SERVO_PIN);
  hw_servo_angle(0);
  hw_ring_color(0, 40, 0);
  /* Wi-Fi: the ESP32 radio joins Wokwi-GUEST; no GPIO pins are used. */
  hw_wifi_connect();

  /* MQTT: application protocol carried over the Wi-Fi connection. */
  hw_mqtt_begin();

  hw_lcd_print(0, "Smart Home Ready");
  hw_lcd_print(1, "Wait 5s to arm");
}

void run_system(void) {
  /* Keep the MQTT connection alive before publishing sensor data. */
  hw_mqtt_connect();
  hw_mqtt_loop();

  /* DHT22 on GPIO 15: proprietary single-wire temperature/humidity data. */
  float temp = hw_read_temperature();
  float hum = hw_read_humidity();

  /* ESP32 ADC inputs: gas AO GPIO 39, LDR AO GPIO 36, water GPIO 33. */
  int gas_raw = hw_analog_read(GAS_AO_PIN);
  int light_raw = hw_analog_read(LDR_PIN);
  int water_raw = hw_analog_read(WATER_PIN);

  /* Digital sensors: MQ2 DO, flame DO, PIR OUT and panic button. */
  bool gas_digital = hw_digital_read(GAS_DO_PIN) == HW_LOW;
  bool flame = hw_digital_read(FLAME_PIN) == HW_LOW;
  bool motion = hw_digital_read(PIR_PIN) == HW_HIGH;
  bool panic = hw_digital_read(PANIC_PIN) == HW_LOW;

  /* HC-SR04 uses separate TRIG and ECHO lines. */
  float distance = read_distance_cm();

  /* Compare readings with the simple teaching thresholds above. */
  bool hot = !isnan(temp) && temp > TEMP_LIMIT_C;
  bool gas = gas_raw > GAS_LIMIT || gas_digital;
  bool water = water_raw > WATER_LIMIT;
  bool dark = light_raw > DARK_LIMIT;
  bool armed = hw_millis() > SAFE_START_MS;
  alarm_active = armed && (hot || gas || flame || water || panic);

  if (alarm_active) {
    const char* reason = hot ? "HIGH TEMP" : gas ? "GAS" : flame ? "FLAME" : water ? "WATER" : "PANIC";

    /* Alarm outputs: cut relay, open servo, beep and show red. */
    hw_digital_write(RELAY_PIN, HW_LOW);
    hw_servo_angle(90);
    hw_tone(BUZZER_PIN, 1200);
    hw_ring_color(180, 0, 0);

    /* HTTP: send one JSON alert over Wi-Fi when an alarm begins. */
    if (!alert_sent) {
      hw_http_alert(reason);
      alert_sent = true;
    }
  } else {
    /* Normal outputs: relay on, distance controls door, buzzer silent. */
    hw_digital_write(RELAY_PIN, HW_HIGH);
    hw_servo_angle(distance < NEAR_DOOR_CM ? 90 : 0);
    hw_no_tone(BUZZER_PIN);

    /* Ring: orange for motion, blue for darkness, green otherwise. */
    if (motion) hw_ring_color(180, 120, 0);
    else if (dark) hw_ring_color(0, 0, 80);
    else hw_ring_color(0, 40, 0);
    alert_sent = false;
  }

  /* MQTT: publish selected sensor values and alarm state over Wi-Fi. */
  if (hw_mqtt_connected()) {
    publish_float("smart-home/temp", temp);
    publish_value("smart-home/gas", gas_raw);
    publish_value("smart-home/light", light_raw);
    hw_mqtt_publish("smart-home/alarm", alarm_active ? "ON" : "OFF");
  }

  /* I2C LCD: update all four rows with current values and RTC time. */
  char line[24];
  snprintf(line, sizeof(line), "T:%.1fC H:%.0f%%", temp, hum);
  hw_lcd_print(0, line);
  snprintf(line, sizeof(line), "Gas:%d Water:%d", gas_raw, water_raw);
  hw_lcd_print(1, line);
  snprintf(line, sizeof(line), "P:%c L:%d D:%.0f", motion ? 'Y' : 'N', light_raw, distance);
  hw_lcd_print(2, line);
  snprintf(line, sizeof(line), alarm_active ? "ALARM ACTIVE" : "Safe %02d:%02d", hw_rtc_hour(), hw_rtc_minute());
  hw_lcd_print(3, line);

  /* UART output for students watching the PlatformIO Serial Monitor. */
  hw_serial_status(temp, gas_raw, alarm_active);
  hw_delay_ms(2000);
}
