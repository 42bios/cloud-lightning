/*
 * Copyright (c) 2015 Molly Nicholas
 * All rights reserved. See LICENSE for the full license text.
 *
 * Natural-looking lightning for a NeoPixel cloud.
 *
 * Shared by cloud-lightning (Arduino) and cloud-lightning-photon (Particle).
 * Both copies of this file must stay identical.
 * Include the NeoPixel library before including this file.
 *
 * A real flash consists of a faint stepped leader, a bright return stroke and
 * a few weaker subsequent strokes 30-120 ms apart, each glowing out quickly.
 * Inside a cloud the light spreads to the neighbouring LEDs and the channel
 * sometimes wanders. Pauses between flashes are exponentially distributed:
 * mostly short, occasionally long.
 */

#ifndef CLOUD_LIGHTNING_H
#define CLOUD_LIGHTNING_H

#include <math.h>

// Slightly blue-white, like lightning seen through a cloud.
const uint8_t LIGHTNING_R = 190;
const uint8_t LIGHTNING_G = 200;
const uint8_t LIGHTNING_B = 255;

// Mean pause between two flashes.
const unsigned long STORM_MEAN_PAUSE_MS = 1200;
const unsigned long AMBIENT_MEAN_PAUSE_MS = 8000;

// Flashes at or above this peak are close strikes (loud thunder),
// below it they are distant sheet lightning (no audible thunder).
const uint8_t CLOSE_STRIKE_PEAK = 128;

class CloudLightning {
public:
  explicit CloudLightning(Adafruit_NeoPixel &s) : strip(s) {}

  // Starts a short thunderstorm of 3-6 flashes.
  void startStorm() {
    stormFlashesLeft = random(3, 7);
    nextStormFlashAt = millis();
  }

  // Ambient mode: endless mix of distant sheet lightning and occasional strikes.
  void setAmbient(bool on) {
    ambientOn = on;
    if (on) {
      nextAmbientFlashAt = millis() + randomPause(AMBIENT_MEAN_PAUSE_MS);
    }
  }

  bool ambient() const {
    return ambientOn;
  }

  void stop() {
    stormFlashesLeft = 0;
    ambientOn = false;
    show(0, 0, 0);
  }

  // Call from loop(). Returns the peak brightness (0-255) of the flash that
  // just happened, or 0 if there was none. A higher peak means a closer strike.
  // A flash blocks for at most ~1.5 s.
  uint8_t update() {
    if (stormFlashesLeft > 0) {
      if ((long)(millis() - nextStormFlashAt) < 0) {
        return 0;
      }
      stormFlashesLeft--;
      uint8_t peak = flash(random(180, 256), random(2, 6), 1);
      nextStormFlashAt = millis() + randomPause(STORM_MEAN_PAUSE_MS);
      return peak;
    }

    if (ambientOn && (long)(millis() - nextAmbientFlashAt) >= 0) {
      uint8_t peak;
      if (random(4) == 0) {
        peak = flash(random(CLOSE_STRIKE_PEAK, 256), random(1, 5), 1);
      } else {
        // Distant sheet lightning: dim, diffuse, single stroke.
        peak = flash(random(50, CLOSE_STRIKE_PEAK), 1, 0);
      }
      nextAmbientFlashAt = millis() + randomPause(AMBIENT_MEAN_PAUSE_MS);
      return peak;
    }

    return 0;
  }

private:
  Adafruit_NeoPixel &strip;
  uint8_t stormFlashesLeft = 0;
  bool ambientOn = false;
  unsigned long nextStormFlashAt = 0;
  unsigned long nextAmbientFlashAt = 0;

  // falloff: how fast the light fades towards the neighbouring LEDs
  // (0 = whole cloud evenly lit, 1 = half per LED, ...).
  uint8_t flash(uint8_t peak, uint8_t strokes, uint8_t falloff) {
    int center = random(strip.numPixels());

    // Stepped leader: a few faint flickers before the main stroke.
    for (int i = random(2, 6); i > 0; i--) {
      show(center, random(peak / 6, peak / 3 + 1), falloff);
      delay(random(5, 25));
    }

    uint8_t level = peak;
    for (uint8_t s = 0; s < strokes; s++) {
      show(center, level, falloff);
      delay(random(10, 40));

      // Afterglow: exponential decay.
      for (int v = level * 7 / 10; v > 6; v = v * 7 / 10) {
        show(center, v, falloff);
        delay(10);
      }
      show(center, 0, falloff);

      if (s + 1 < strokes) {
        delay(random(30, 120));
        // Subsequent strokes are weaker than the first one.
        level = peak * random(45, 86) / 100;
        // The channel sometimes wanders to a neighbouring part of the cloud.
        if (random(3) == 0) {
          center = constrain(center + (int)random(-1, 2), 0, (int)strip.numPixels() - 1);
        }
      }
    }
    return peak;
  }

  void show(int center, uint8_t level, uint8_t falloff) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      int distance = abs((int)i - center);
      int shift = distance * falloff;
      uint8_t v = shift >= 8 ? 0 : perceived(level >> shift);
      strip.setPixelColor(i, strip.Color((uint16_t)v * LIGHTNING_R / 255,
                                         (uint16_t)v * LIGHTNING_G / 255,
                                         (uint16_t)v * LIGHTNING_B / 255));
    }
    strip.show();
  }

  // Gamma 2.0: the eye perceives brightness non-linearly, so this makes
  // fades look smooth instead of stepping down abruptly.
  static uint8_t perceived(uint8_t v) {
    return (uint16_t)v * v / 255;
  }

  // Exponentially distributed pause with the given mean, at least 250 ms.
  static unsigned long randomPause(unsigned long meanMs) {
    return 250 + (unsigned long)(-log(random(1, 1001) / 1000.0) * meanMs);
  }
};

#endif
