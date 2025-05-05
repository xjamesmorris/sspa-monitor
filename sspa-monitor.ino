/*
 * sspa-monitor (c) 2024, James Morris W7TXT <jmorris@namei.org>
 *
 * This version supports measurement of drain current, drain voltage, and heatsink temperature.
 * I developed it for initial testing of a 30W GAN SSPA for 10 GHz, to set Idq & monitor temperature.
 *
 * Todo:
 *    - Fan control
 *    - Wireless telemetry
 *    - Vg & Ig measurement
 *    - RF power monitoring
 *
 *
 * Thermistor support based on ladyada's code, all other code is GPL.
 *
 * My configuration:
 *    MCU:               Sparkfun Arduino Pro Mini
 *    OLED:              Seeed 0.96" SSD1315 128x64 pixels
 *    Thermistor:        NRG1104H3950B1H, 10k, +/-3%, 3950 beta, 25/50 type.
 *    Current sensor:    INA169
 *
 */
#include <Wire.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <LibPrintf.h>
#include "sspa-monitor.h"

#define VERSION F("v0.42")
#define LINE_MAX 12
#define SPLASH_DELAY 800
#define LOOP_DELAY 250

/* Current measurement via an INA169, 1v/1A. */
#define CURRENT_PIN A0
#define V_RAW_MAX 1023

/* Drain voltage measurement, 0.08V/V on this setup. Todo: replace 100k R w/ 91k */
#define V_DRAIN_PIN A2
#define V_DRAIN_GAIN 12.5 /* 1/0.08 */

/* Rs = 0.1 ohms. Max I = 5A. Full scale Vshunt = (Rs x I) = 500mV. */
#define C_GAIN 10

#define V_REF 5.00 /* measured Vcc */
#define V_GAIN 1

/* Splash display parameters */
#define OLED_SPLASH_FONT u8g2_font_7x13_mf
#define OLED_SPLASH_LINE_1 24
#define OLED_SPLASH_LINE_2 (OLED_SPLASH_LINE_1 + 24)

/* Data display parameters */
#define OLED_DATA_FONT u8g2_font_9x15_mf
#define OLED_DATA_LINE_1 18
#define OLED_DATA_LINE_2 (OLED_DATA_LINE_1 + 20)
#define OLED_DATA_LINE_3 (OLED_DATA_LINE_2 + 20)

#define OLED_UPDATE_INTERVAL 500    /* ms */
#define SERIAL_UPDATE_INTERVAL 1000 /* ms */

unsigned long oled_update_last;
unsigned long serial_update_last;

struct telemetry_element {
  double val;
  double last;
};

struct telemetry_data {
  struct telemetry_element t_heatsink;
  struct telemetry_element i_drain;
  struct telemetry_element v_drain;
} td;

/* Use slower page-based transfer, otherwise we will run out of memory */
U8G2_SSD1306_128X64_NONAME_2_SW_I2C oled(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

/* for snprintf */
char oled_buf[LINE_MAX];

bool element_changed(struct telemetry_element *te) {
  return (te->val != te->last);
}

bool telemetry_changed(struct telemetry_data *td) {
  return (element_changed(&td->t_heatsink) || element_changed(&td->i_drain) || element_changed(&td->v_drain));
}

void element_update(struct telemetry_element *te) {
  if (te->val != te->last)
    te->last = te->val;
}

void telemetry_update(struct telemetry_data *td) {
  element_update(&td->t_heatsink);
  element_update(&td->i_drain);
  element_update(&td->v_drain);
}

void oled_splash(void) {

  oled.firstPage();

  do {
    oled.setFont(OLED_SPLASH_FONT);
    oled.setCursor(0, OLED_SPLASH_LINE_1);
    oled.print(F("sspa-monitor "));
    oled.print(VERSION);
    oled.setCursor(0, OLED_SPLASH_LINE_2);
    oled.print(F("      W7TXT"));
  } while (oled.nextPage());

  delay(SPLASH_DELAY);
}

void oled_update(struct telemetry_data *td) {

  oled.firstPage();

  do {
    oled.setFont(OLED_DATA_FONT);

    oled.setCursor(0, OLED_DATA_LINE_1);
    snprintf(oled_buf, LINE_MAX, "Vd: %7.2f", td->v_drain.val);
    oled.print(oled_buf);

    oled.setCursor(0, OLED_DATA_LINE_2);
    snprintf(oled_buf, LINE_MAX, "Id: %7.2f", td->i_drain.val);
    oled.print(oled_buf);

    oled.setCursor(0, OLED_DATA_LINE_3);
    snprintf(oled_buf, LINE_MAX, "Th: %7.2f", td->t_heatsink.val);
    oled.print(oled_buf);

  } while (oled.nextPage());
}

void serial_splash(void) {
  Serial.print(F("\nsspa-monitor "));
  Serial.println(VERSION);
  Serial.println(F("Vd (V)\tId (A)\tTh (°C)"));
}

void serial_update(struct telemetry_data *td) {
  Serial.print(td->t_heatsink.val);
  Serial.print(F("\t"));
  Serial.print(td->i_drain.val);
  Serial.print(F("\t"));
  Serial.print(td->v_drain.val);
  Serial.println(F(""));
}

void oled_setup(void) {
  oled.begin();
  oled.enableUTF8Print();
}

void telemetry_setup(struct telemetry_data *td) {
  memset(td, 0, sizeof(*td));
}

void display_splash(void) {
  serial_splash();
  oled_splash();
}

void setup(void) {

  Serial.begin(38400);
  while (!Serial)
    ;

  telemetry_setup(&td);

  /* Use Vcc as the reference on this board */
  analogReference(DEFAULT);

  oled_setup();
  display_splash();

  oled_update_last = 0;
  serial_update_last = 0;
}

double get_v_drain(void) {
  int raw_val;
  double val;

  raw_val = analogRead(V_DRAIN_PIN);
  val = raw_val * (V_REF / (double)V_RAW_MAX) * V_DRAIN_GAIN;
  return val;
}

double get_i_drain(void) {
  int raw_val;
  double val;

  raw_val = analogRead(CURRENT_PIN);
  val = raw_val * (V_REF / (double)V_RAW_MAX) * V_GAIN;
  return val;
}

void display_update(struct telemetry_data *td) {
  unsigned long now = millis();

  if (now - serial_update_last > SERIAL_UPDATE_INTERVAL) {
    serial_update(td);
    serial_update_last = now;
  }

  if (now - oled_update_last > OLED_UPDATE_INTERVAL) {
    oled_update(td);
    oled_update_last = now;
  }
}

void loop(void) {

  td.i_drain.val = get_i_drain();
  td.v_drain.val = get_v_drain();
  td.t_heatsink.val = get_t_heatsink();

  display_update(&td);
  telemetry_update(&td);

  delay(LOOP_DELAY);
}
