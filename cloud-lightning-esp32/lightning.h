/*
 * Copyright (c) 2015 Molly Nicholas
 * All rights reserved. See LICENSE for the full license text.
 *
 * Natural-looking lightning for a NeoPixel cloud.
 *
 * Shared by cloud-lightning (Arduino) and cloud-lightning-esp32.
 * Both copies of this file must stay identical.
 * Include the NeoPixel library before including this file.
 *
 * A real flash consists of a faint stepped leader, a bright return stroke and
 * a few weaker subsequent strokes 30-120 ms apart, each glowing out quickly.
 * Some strokes carry a continuing current and keep glowing, flickering, for
 * up to 200 ms. Inside a cloud the light spreads to the neighbouring LEDs and
 * the channel sometimes wanders. Pauses between flashes are exponentially
 * distributed: mostly short, occasionally long.
 *
 * Colour: the channel itself is white with a touch of blue. Light that is
 * scattered through the cloud, comes from far away or is fading out turns
 * violet, so the LEDs around the channel, distant sheet lightning and the
 * afterglow are more violet than the core of a close flash.
 *
 * Every flash has a distance: close strikes are bright with several strokes,
 * distant ones are faint, diffuse sheet lightning.
 *
 * Several clouds share one storm: one cloud (the leader) decides the flashes
 * and sends them to the others. Each flash starts in one cloud and is either
 * - an intra-cloud flash that stays inside that cloud,
 * - a channel that jumps on to the neighbouring clouds in one direction,
 *   a moment later and weaker in each of them, or
 * - a big strike that also lights the other clouds faintly from the side.
 * The timing comes from the flash's seed, so all clouds keep the rhythm of
 * the one flash; where it flickers and how bright differs in every cloud.
 */

#ifndef CLOUD_LIGHTNING_H
#define CLOUD_LIGHTNING_H

#include <math.h>

// Colour of the lightning channel and of light scattered through the cloud.
const uint8_t CORE_R = 235, CORE_G = 238, CORE_B = 255;
const uint8_t SCATTER_R = 150, SCATTER_G = 125, SCATTER_B = 255;

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

// Several clouds: delay per cloud when the channel jumps on to a neighbour.
const unsigned long CLOUD_JUMP_DELAY_MS = 60;

const uint8_t AFTERGLOW_STEPS = 8;
const unsigned long AFTERGLOW_STEP_MS = 10;
const unsigned long CONTINUING_STEP_MS = 15;

struct Flash {
  float km;          // distance of the strike
  uint8_t strokes;   // number of return strokes
  uint8_t origin;    // cloud the flash starts in (0 for a single cloud)
  uint8_t spread;    // how many clouds the channel jumps on to
  int8_t direction;  // direction of the jump: -1 left, 1 right, 0 both
  bool glow;         // the other clouds are lit faintly from the side
  uint32_t seed;     // same seed -> same timing on every cloud
};

// Small deterministic random generator (xorshift32), so every cloud computes
// the same flash identically, independent of the platform's random().
class FlashRandom {
public:
  explicit FlashRandom(uint32_t seed) : state(seed ? seed : 1) {}

  // Random number in [low, high).
  long range(long low, long high) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return low + (long)(state % (uint32_t)(high - low));
  }

private:
  uint32_t state;
};

class CloudLightning {
public:
  explicit CloudLightning(Adafruit_NeoPixel &s) : strip(s) {}

  // Several clouds: number of clouds in the installation and the position
  // of this one (0 .. cloudCount-1).
  void setLayout(uint8_t cloudCount, uint8_t cloudPosition) {
    clouds = cloudCount > 0 ? cloudCount : 1;
    position = cloudPosition;
  }

  uint8_t cloudPosition() const {
    return position;
  }

  // Starts a short thunderstorm of 3-6 flashes.
  void startStorm() {
    stormFlashesLeft = random(3, 7);
    nextStormFlashAt = millis();
  }

  bool stormActive() const {
    return stormFlashesLeft > 0;
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
    if (km < 0 || queued >= STRIKE_QUEUE_SIZE) {
      return;
    }
    queue[queued++] = km;
  }

  void stop() {
    stormFlashesLeft = 0;
    ambientOn = false;
    queued = 0;
    off();
  }

  void off() {
    fill(0, 0, 0);
  }

  // Lights all LEDs in one colour, e.g. for status feedback.
  void fill(uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
  }

  // Standalone use: call from loop(). Decides and shows the next flash.
  // Returns true and fills `flash` when a flash happened (it blocks for a
  // few hundred ms, at most ~2.5 s), false otherwise.
  bool update(Flash &flash) {
    if (!poll(flash)) {
      return false;
    }
    render(flash);
    finished();
    return true;
  }

  // Leader of several clouds: poll(), send the flash to the other clouds,
  // render() it and call finished() afterwards.
  bool poll(Flash &flash) {
    float km;
    if (queued > 0 && (long)(millis() - nextQueuedAt) >= 0) {
      km = queue[0];
      queued--;
      for (uint8_t i = 0; i < queued; i++) {
        queue[i] = queue[i + 1];
      }
      source = FROM_STRIKE;
    } else if (stormFlashesLeft > 0 && (long)(millis() - nextStormFlashAt) >= 0) {
      stormFlashesLeft--;
      km = random(5, 41) / 10.0;                          // 0.5-4 km: right above us
      source = FROM_STORM;
    } else if (ambientOn && (long)(millis() - nextAmbientFlashAt) >= 0) {
      km = random(4) == 0 ? random(20, 101) / 10.0        // occasional strike 2-10 km
                          : random(20, 61);               // mostly 20-60 km away
      source = FROM_AMBIENT;
    } else {
      return false;
    }

    km = (long)(km * 100 + 0.5) / 100.0;  // the precision the other clouds receive
    bool close = km < CLOSE_STRIKE_KM;
    flash.km = km;
    flash.strokes = close ? random(2, 6) : 1;
    flash.origin = random(clouds);
    flash.spread = 0;
    flash.direction = 0;
    flash.glow = true;  // distant sheet lightning lights up the whole sky
    if (close && clouds > 1) {
      long kind = random(10);
      if (kind < 4) {
        flash.glow = false;                               // stays inside one cloud
      } else if (kind < 8) {
        flash.spread = random(1, clouds);                 // jumps on to neighbours
        flash.direction = random(-1, 2);
      }                                                   // else: big strike
    }
    flash.seed = random(1, 0x7FFFFFFF);
    return true;
  }

  // Schedules the next flash after the current one has been shown.
  void finished() {
    switch (source) {
      case FROM_STRIKE:
        nextQueuedAt = millis() + STRIKE_MIN_GAP_MS;
        break;
      case FROM_STORM:
        nextStormFlashAt = millis() + randomPause(STORM_MEAN_PAUSE_MS);
        break;
      case FROM_AMBIENT:
        nextAmbientFlashAt = millis() + randomPause(AMBIENT_MEAN_PAUSE_MS);
        break;
    }
  }

  // Shows a flash as seen from this cloud. Blocks for at most ~2.5 s.
  void render(const Flash &flash) {
    // `timing` is drawn identically on every cloud, so all clouds keep the
    // rhythm of the flash. `local` differs per cloud: where it flickers,
    // how bright and which colour.
    FlashRandom timing(flash.seed);
    FlashRandom local(flash.seed ^ (0x9E3779B9UL * (position + 1)));
    uint16_t pixels = strip.numPixels();

    float closeness = 1 - constrain(flash.km / MAX_STRIKE_KM, 0.0f, 1.0f);
    int peak = 50 + closeness * closeness * 205 * local.range(80, 101) / 100;
    uint8_t falloff = flash.km < CLOSE_STRIKE_KM ? 1 : 0;
    int center = local.range(0, pixels);
    // How violet the flash looks: 0 = channel colour, 255 = scattered light.
    int scatter = (1 - closeness) * 200 + local.range(0, 40);
    bool sideLit = false;

    int offset = (int)position - (int)flash.origin;
    if (offset != 0) {
      int distance = abs(offset);
      bool onPath = flash.direction == 0 || (offset > 0) == (flash.direction > 0);
      falloff = 0;
      if (distance <= flash.spread && onPath) {
        // The channel jumps on to this cloud a moment later, weaker.
        peak >>= distance;
        scatter += 50 * distance;
        delay(distance * CLOUD_JUMP_DELAY_MS);
      } else if (flash.glow) {
        peak = peak * local.range(8, 20) / 100;
        scatter += 150;
        sideLit = true;
      } else {
        return;  // the flash stays in another cloud
      }
    }
    tint = constrain(scatter, 0, 255);

    // Stepped leader: a few faint flickers before the main stroke.
    for (int i = timing.range(2, 6); i > 0; i--) {
      show(center, local.range(peak / 6, peak / 3 + 1), falloff);
      delay(timing.range(5, 25));
    }

    int level = peak;
    for (uint8_t s = 0; s < flash.strokes; s++) {
      // From the side, not every stroke is visible.
      bool visible = !sideLit || local.range(0, 3) != 0;
      show(center, visible ? level : 0, falloff);
      delay(timing.range(10, 40));

      // Continuing current: the channel keeps glowing and flickering.
      if (timing.range(0, 5) == 0) {
        long steps = timing.range(3, 14);
        for (long i = 0; i < steps; i++) {
          show(center, visible ? level * local.range(55, 90) / 100 : 0, falloff);
          delay(CONTINUING_STEP_MS);
        }
      }

      // Afterglow: exponential decay, same duration on every cloud.
      int v = visible ? level : 0;
      for (uint8_t step = 0; step < AFTERGLOW_STEPS; step++) {
        v = v * 7 / 10;
        show(center, v > 6 ? v : 0, falloff);
        delay(AFTERGLOW_STEP_MS);
      }
      show(center, 0, falloff);

      if (s + 1 < flash.strokes) {
        delay(timing.range(30, 120));
        // Subsequent strokes are weaker than the first one.
        level = peak * local.range(45, 86) / 100;
        // The channel sometimes wanders to a neighbouring part of the cloud.
        if (local.range(0, 3) == 0) {
          center = constrain(center + (int)local.range(-1, 2), 0, (int)pixels - 1);
        }
      }
    }
  }

private:
  enum Source { FROM_STRIKE, FROM_STORM, FROM_AMBIENT };

  Adafruit_NeoPixel &strip;
  uint8_t clouds = 1;
  uint8_t position = 0;
  uint8_t stormFlashesLeft = 0;
  bool ambientOn = false;
  unsigned long nextStormFlashAt = 0;
  unsigned long nextAmbientFlashAt = 0;
  float queue[STRIKE_QUEUE_SIZE];
  uint8_t queued = 0;
  unsigned long nextQueuedAt = 0;
  Source source = FROM_STRIKE;
  uint8_t tint = 0;  // scatter colour of the current flash

  // falloff: how fast the light fades towards the neighbouring LEDs
  // (0 = whole cloud evenly lit, 1 = half per LED, ...). LEDs away from the
  // channel and fading light get more violet.
  void show(int center, uint8_t level, uint8_t falloff) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      int distance = abs((int)i - center);
      int shift = distance * falloff;
      uint8_t v = shift >= 8 ? 0 : perceived(level >> shift);
      int mix = constrain(tint + distance * falloff * 70 + (255 - level) / 3, 0, 255);
      strip.setPixelColor(i, strip.Color(blend(CORE_R, SCATTER_R, mix, v),
                                         blend(CORE_G, SCATTER_G, mix, v),
                                         blend(CORE_B, SCATTER_B, mix, v)));
    }
    strip.show();
  }

  // Colour channel between core and scatter colour (mix 0-255), scaled to v.
  static uint8_t blend(uint8_t core, uint8_t scatter, int mix, uint8_t v) {
    uint16_t c = ((uint32_t)core * (255 - mix) + (uint32_t)scatter * mix) / 255;
    return c * v / 255;
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
