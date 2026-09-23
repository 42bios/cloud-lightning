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
 *
 * Every flash has a distance: close strikes are bright with several strokes,
 * distant ones are faint, diffuse sheet lightning.
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

// Strikes closer than this get several strokes and light up a part of the
// cloud; farther ones are single-stroke sheet lightning lighting all LEDs.
const float CLOSE_STRIKE_KM = 15;
// Strikes at or beyond this distance are shown at the minimum brightness.
const float MAX_STRIKE_KM = 60;

// Real strikes (strikeAt) waiting to be shown.
const uint8_t STRIKE_QUEUE_SIZE = 4;
const unsigned long STRIKE_MIN_GAP_MS = 300;

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

  // Shows a real strike at the given distance, e.g. reported by a lightning
  // detection network. Strikes are dropped while the queue is full.
  void strikeAt(float km) {
    if (km < 0 || queuedStrikes >= STRIKE_QUEUE_SIZE) {
      return;
    }
    strikeQueue[queuedStrikes++] = km;
  }

  void stop() {
    stormFlashesLeft = 0;
    ambientOn = false;
    queuedStrikes = 0;
    show(0, 0, 0);
  }

  // Call from loop(). Returns the distance in km of the flash that just
  // happened, or -1 if there was none. A flash blocks for at most ~1.5 s.
  float update() {
    if (queuedStrikes > 0 && (long)(millis() - nextStrikeAt) >= 0) {
      float km = strikeQueue[0];
      queuedStrikes--;
      for (uint8_t i = 0; i < queuedStrikes; i++) {
        strikeQueue[i] = strikeQueue[i + 1];
      }
      flashAt(km);
      nextStrikeAt = millis() + STRIKE_MIN_GAP_MS;
      return km;
    }

    if (stormFlashesLeft > 0) {
      if ((long)(millis() - nextStormFlashAt) < 0) {
        return -1;
      }
      stormFlashesLeft--;
      float km = random(5, 41) / 10.0;                // 0.5-4 km: right above us
      flashAt(km);
      nextStormFlashAt = millis() + randomPause(STORM_MEAN_PAUSE_MS);
      return km;
    }

    if (ambientOn && (long)(millis() - nextAmbientFlashAt) >= 0) {
      float km = random(4) == 0 ? random(20, 101) / 10.0   // occasional strike 2-10 km
                                : random(20, 61);          // mostly 20-60 km away
      flashAt(km);
      nextAmbientFlashAt = millis() + randomPause(AMBIENT_MEAN_PAUSE_MS);
      return km;
    }

    return -1;
  }

private:
  Adafruit_NeoPixel &strip;
  uint8_t stormFlashesLeft = 0;
  bool ambientOn = false;
  unsigned long nextStormFlashAt = 0;
  unsigned long nextAmbientFlashAt = 0;
  float strikeQueue[STRIKE_QUEUE_SIZE];
  uint8_t queuedStrikes = 0;
  unsigned long nextStrikeAt = 0;

  void flashAt(float km) {
    float closeness = 1 - constrain(km / MAX_STRIKE_KM, 0.0f, 1.0f);
    uint8_t peak = 50 + closeness * closeness * 205 * random(80, 101) / 100;
    if (km < CLOSE_STRIKE_KM) {
      flash(peak, random(2, 6), 1);
    } else {
      flash(peak, 1, 0);
    }
  }

  // falloff: how fast the light fades towards the neighbouring LEDs
  // (0 = whole cloud evenly lit, 1 = half per LED, ...).
  void flash(uint8_t peak, uint8_t strokes, uint8_t falloff) {
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
