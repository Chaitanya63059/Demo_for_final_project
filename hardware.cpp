#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>

/* Pins used directly by the Arduino libraries in this support file. */
#define DHT_PIN 15
#define RING_PIN 2
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define RING_PIXELS 16

#define HW_INPUT 0
#define HW_OUTPUT 1
#define HW_INPUT_PULLUP 2

/* The main teaching program is written in C inside main.c. */
extern "C" void start_system(void);
extern "C" void run_system(void);

/*
 * Arduino library objects.
 * The teaching logic remains in main.c. This file only adapts C functions
 * to the C++ APIs supplied by the Arduino ESP32 libraries.
 */
static DHT dht(DHT_PIN, DHT22);                         /* DHT22 on GPIO 15 */
static LiquidCrystal_I2C lcd(0x27, 20, 4);             /* I2C LCD address 0x27 */
static RTC_DS1307 rtc;                                 /* I2C RTC address 0x68 */
static Servo doorServo;                                /* Servo PWM on GPIO 13 */
static Adafruit_NeoPixel ring(                         /* NeoPixel data on GPIO 2 */
    RING_PIXELS, RING_PIN, NEO_GRB + NEO_KHZ800);
static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

/* Arduino calls setup()/loop(); forward them to the C program. */
void setup() {
  start_system();
}

void loop() {
  run_system();
}

extern "C" {

/* UART0 / USB Serial Monitor bridge. */
void hw_serial_begin(int baud) { Serial.begin(baud); }
void hw_serial_println(const char* text) { Serial.println(text); }
void hw_serial_status(float temp, int gas, bool alarm) {
  Serial.println("Temp=" + String(temp, 1) + " Gas=" + String(gas) + " Alarm=" + String(alarm));
}

/* ESP32 GPIO, ADC, timing and tone bridge. */
void hw_pin_mode(int pin, int mode) {
  pinMode(pin, mode == HW_OUTPUT ? OUTPUT : mode == HW_INPUT_PULLUP ? INPUT_PULLUP : INPUT);
}
void hw_digital_write(int pin, int value) { digitalWrite(pin, value); }
int hw_digital_read(int pin) { return digitalRead(pin); }
int hw_analog_read(int pin) { return analogRead(pin); }
void hw_delay_ms(unsigned long ms) { delay(ms); }
void hw_delay_us(unsigned int us) { delayMicroseconds(us); }
unsigned long hw_millis(void) { return millis(); }
unsigned long hw_pulse_high(int pin, unsigned long timeout_us) { return pulseIn(pin, HIGH, timeout_us); }
void hw_tone(int pin, int frequency) { tone(pin, frequency); }
void hw_no_tone(int pin) { noTone(pin); }

/* I2C bridge: LCD2004 and DS1307 share GPIO 21 SDA / GPIO 22 SCL. */
void hw_i2c_begin(void) { Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); }
void hw_lcd_begin(void) { lcd.init(); lcd.backlight(); }
void hw_lcd_print(int row, const char* text) {
  String line(text);
  while (line.length() < 20) line += ' ';
  lcd.setCursor(0, row);
  lcd.print(line.substring(0, 20));
}
void hw_rtc_begin(void) { rtc.begin(); }
int hw_rtc_hour(void) { return rtc.now().hour(); }
int hw_rtc_minute(void) { return rtc.now().minute(); }

/* DHT22 proprietary single-wire bridge on GPIO 15. */
void hw_dht_begin(void) { dht.begin(); }
float hw_read_temperature(void) { return dht.readTemperature(); }
float hw_read_humidity(void) { return dht.readHumidity(); }

/* NeoPixel single-wire output on GPIO 2 and servo PWM on GPIO 13. */
void hw_ring_begin(void) { ring.begin(); ring.show(); }
void hw_ring_color(uint8_t red, uint8_t green, uint8_t blue) {
  for (int i = 0; i < RING_PIXELS; i++) ring.setPixelColor(i, ring.Color(red, green, blue));
  ring.show();
}
void hw_servo_begin(int pin) { doorServo.attach(pin); }
void hw_servo_angle(int angle) { doorServo.write(angle); }

/* Wi-Fi uses the ESP32 radio, so it does not consume an external GPIO. */
void hw_wifi_connect(void) {
  WiFi.begin("Wokwi-GUEST", "");
  Serial.print("WiFi");
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " offline");
}
bool hw_wifi_connected(void) { return WiFi.status() == WL_CONNECTED; }

/* MQTT runs over Wi-Fi and publishes telemetry to a public test broker. */
void hw_mqtt_begin(void) { mqtt.setServer("broker.hivemq.com", 1883); }
void hw_mqtt_connect(void) {
  if (hw_wifi_connected() && !mqtt.connected()) mqtt.connect("wokwi-smart-home-c-demo");
}
void hw_mqtt_loop(void) { mqtt.loop(); }
bool hw_mqtt_connected(void) { return mqtt.connected(); }
void hw_mqtt_publish(const char* topic, const char* value) { mqtt.publish(topic, value); }

/* HTTP also runs over Wi-Fi; httpbin is used only as a demonstration endpoint. */
void hw_http_alert(const char* reason) {
  if (!hw_wifi_connected()) return;
  HTTPClient http;
  http.begin("http://httpbin.org/post");
  http.addHeader("Content-Type", "application/json");
  http.POST(String("{\"alert\":\"") + reason + "\"}");
  http.end();
}

}
