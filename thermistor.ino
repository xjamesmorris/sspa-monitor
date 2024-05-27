/*
 * Thermistor support based on ladyada's code:
 *    SPDX-FileCopyrightText: 2011 Limor Fried/ladyada for Adafruit Industries
 *    SPDX-License-Identifier: MIT
 *    thermistor-2.ino Intermediate test program for a thermistor. Adafruit Learning System Tutorial
 *    https://learn.adafruit.com/thermistor/using-a-thermistor by Limor Fried, Adafruit Industries
 *    MIT License - please keep attribution and please consider buying parts from Adafruit
*/
#include "sspa-monitor.h"

#define THERMISTOR_PIN A1
#define THERMISTOR_NOMINAL 10000
#define TEMPERATURE_NOMINAL 25 /* temp. for nominal resistance (almost always 25 C) */
#define B_COEFFICIENT 3950     /* The beta coefficient of the thermistor (usually 3000-4000) */
#define SERIES_RESISTOR 9940   /* Value of 'other' resistor. (Measured on board.) */
#define N_SAMPLES 10

int t_heatskink_samples[N_SAMPLES];

float get_t_heatsink(void) {
  int i;
  float avg, steinhart;

  /* take N samples in a row, with a slight delay */
  for (i = 0; i < N_SAMPLES; i++) {
    t_heatskink_samples[i] = analogRead(THERMISTOR_PIN);
    delay(10);
  }

  /* average all the samples out */
  avg = 0;
  for (i = 0; i < N_SAMPLES; i++) {
    avg += t_heatskink_samples[i];
  }
  avg /= N_SAMPLES;

  /* convert the value to resistance */
  avg = (1023 / avg) - 1;
  avg = SERIES_RESISTOR / avg;

  steinhart = avg / THERMISTOR_NOMINAL;               // (R/Ro)
  steinhart = log(steinhart);                         // ln(R/Ro)
  steinhart /= B_COEFFICIENT;                         // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);  // + (1/To)
  steinhart = 1.0 / steinhart;                        // Invert
  steinhart -= 273.15;                                // convert absolute temp to C
  return steinhart;
}
