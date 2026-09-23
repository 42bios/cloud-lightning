/*
 * Copyright (c) 2015 Molly Nicholas
 * All rights reserved. See LICENSE for the full license text.
 *
 * Optional thunder: sound via a DFPlayer Mini MP3 module (Thunder) and a
 * vibration motor that rumbles with close thunder (Rumble).
 *
 * Shared by cloud-lightning (Arduino) and cloud-lightning-esp32.
 * Both copies of this file must stay identical.
 *
 * Thunder follows the flash with the real delay of sound (about 3 s per km)
 * and gets quieter with distance; beyond THUNDER_MAX_KM it stays silent.
 *
 * Like real thunder, it also changes with the flash:
 * - close strikes crack sharply, distant ones only rumble, so the sound is
 *   picked from one of three folders on the SD card:
 *     /01/001.mp3, /01/002.mp3 ...  close: sharp crack   (< 3 km)
 *     /02/001.mp3 ...               medium: rolling thunder (3-8 km)
 *     /03/001.mp3 ...               distant: low rumble  (> 8 km)
 * - it lasts longer the farther away the strike is (the sound of the long
 *   channel arrives spread out over time) and the more strokes it had, then
 *   fades out. Use sound files at least 15 s long.
 *
 * The vibration motor only rumbles with close thunder (< RUMBLE_MAX_KM):
 * a hard jolt for a crack, then a decaying, uneven rumble.
 */

#ifndef CLOUD_THUNDER_H
#define CLOUD_THUNDER_H

const float SPEED_OF_SOUND_KM_PER_S = 0.343;
const float THUNDER_MAX_KM = 15;     // thunder is rarely audible beyond this
const float THUNDER_CLOSE_KM = 3;
const float THUNDER_MEDIUM_KM = 8;
const uint8_t THUNDER_TRACKS[3] = {3, 3, 3};  // number of files in /01, /02, /03
const unsigned int THUNDER_MIN_MS = 2500;
const unsigned int THUNDER_MAX_MS = 15000;
const unsigned long THUNDER_FADE_STEP_MS = 100;
const uint8_t THUNDER_QUEUE_SIZE = 4;

const float RUMBLE_MAX_KM = 8;       // only close thunder can be felt
const uint8_t RUMBLE_MIN_PWM = 70;   // below this most motors don't turn
const unsigned long RUMBLE_STEP_MS = 30;

// When the thunder of a strike at the given distance arrives.
inline unsigned long thunderDelayMs(float km) {
  return (unsigned long)(km / SPEED_OF_SOUND_KM_PER_S * 1000);
}

// How long the thunder of a strike rolls.
inline unsigned int thunderDurationMs(float km, uint8_t strokes) {
  unsigned int duration = THUNDER_MIN_MS + km * 700 + strokes * 600;
  return constrain(duration, THUNDER_MIN_MS, THUNDER_MAX_MS);
}

class Thunder {
public:
  // serial: a 9600 baud connection to the DFPlayer, already started.
  explicit Thunder(Stream &serial) : player(serial) {}

  void setEnabled(bool on) {
    enabled = on;
    if (!on) {
      cancel();
    }
  }

  bool isEnabled() const {
    return enabled;
  }

  void cancel() {
    pending = 0;
    if (playing) {
      send(0x16, 0);  // stop
      playing = false;
    }
  }

  // Schedules the thunder for a flash at the given distance.
  void strikeAt(float km, uint8_t strokes) {
    if (!enabled || km < 0 || km > THUNDER_MAX_KM || pending >= THUNDER_QUEUE_SIZE) {
      return;
    }
    Pending &p = queue[pending++];
    p.at = millis() + thunderDelayMs(km);
    p.volume = 30 - (uint8_t)(km / THUNDER_MAX_KM * 20);  // 30 close, 10 far
    p.folder = km < THUNDER_CLOSE_KM ? 1 : km < THUNDER_MEDIUM_KM ? 2 : 3;
    p.durationMs = thunderDurationMs(km, strokes);
  }

  // Call from loop(). Starts thunder that is due and fades out the current one.
  void update() {
    for (uint8_t i = 0; i < pending; i++) {
      if ((long)(millis() - queue[i].at) >= 0) {
        play(queue[i]);
        queue[i] = queue[--pending];
        return;
      }
    }

    if (playing && (long)(millis() - fadeAt) >= 0) {
      if (volume <= 2) {
        send(0x16, 0);  // stop
        playing = false;
      } else {
        volume -= 2;
        send(0x06, volume);
        fadeAt = millis() + THUNDER_FADE_STEP_MS;
      }
    }
  }

private:
  struct Pending {
    unsigned long at;
    unsigned int durationMs;
    uint8_t volume;
    uint8_t folder;
  };

  Stream &player;
  bool enabled = true;
  Pending queue[THUNDER_QUEUE_SIZE];
  uint8_t pending = 0;
  bool playing = false;
  uint8_t volume = 0;
  unsigned long fadeAt = 0;

  void play(const Pending &p) {
    volume = p.volume;
    send(0x06, volume);                     // set volume (0-30)
    delay(30);                              // DFPlayer needs a short gap between commands
    uint8_t file = random(1, THUNDER_TRACKS[p.folder - 1] + 1);
    send(0x0F, (uint16_t)p.folder << 8 | file);  // play /<folder>/<file>.mp3
    playing = true;
    fadeAt = millis() + p.durationMs;
  }

  // DFPlayer serial frame: start, version, length, command, feedback,
  // parameter (2 bytes), checksum (2 bytes), end.
  void send(uint8_t command, uint16_t parameter) {
    uint8_t frame[10] = {0x7E, 0xFF, 0x06, command, 0x00,
                         (uint8_t)(parameter >> 8), (uint8_t)parameter, 0, 0, 0xEF};
    uint16_t sum = 0;
    for (uint8_t i = 1; i < 7; i++) {
      sum += frame[i];
    }
    uint16_t checksum = -sum;
    frame[7] = checksum >> 8;
    frame[8] = checksum & 0xFF;
    player.write(frame, sizeof(frame));
  }
};

// Vibration motor on a PWM pin, driven through a transistor or MOSFET with a
// flyback diode (never directly from the pin).
class Rumble {
public:
  explicit Rumble(uint8_t motorPin) : pin(motorPin) {}

  void begin() {
    pinMode(pin, OUTPUT);
    analogWrite(pin, 0);
  }

  void setEnabled(bool on) {
    enabled = on;
    if (!on) {
      cancel();
    }
  }

  void cancel() {
    pending = 0;
    active = false;
    analogWrite(pin, 0);
  }

  // Schedules the rumble for a flash at the given distance.
  void strikeAt(float km, uint8_t strokes) {
    if (!enabled || km < 0 || km > RUMBLE_MAX_KM || pending >= THUNDER_QUEUE_SIZE) {
      return;
    }
    Pending &p = queue[pending++];
    p.at = millis() + thunderDelayMs(km);
    p.durationMs = thunderDurationMs(km, strokes) / 2;  // felt shorter than heard
    p.strength = 255 - (uint8_t)(km / RUMBLE_MAX_KM * 120);
    p.crack = km < THUNDER_CLOSE_KM;
  }

  // Call from loop().
  void update() {
    for (uint8_t i = 0; i < pending; i++) {
      if ((long)(millis() - queue[i].at) >= 0) {
        current = queue[i];
        queue[i] = queue[--pending];
        active = true;
        startedAt = millis();
        lastStepAt = 0;
        break;
      }
    }

    if (!active || millis() - lastStepAt < RUMBLE_STEP_MS) {
      return;
    }
    lastStepAt = millis();
    unsigned long t = millis() - startedAt;
    if (t >= current.durationMs) {
      analogWrite(pin, 0);
      active = false;
      return;
    }
    // A crack starts with a hard jolt, then the rumble decays unevenly.
    float envelope = current.crack && t < 150 ? 1.0 : exp(-3.0 * t / current.durationMs);
    int level = current.strength * envelope * random(45, 101) / 100;
    analogWrite(pin, level >= RUMBLE_MIN_PWM ? level : 0);
  }

private:
  struct Pending {
    unsigned long at;
    unsigned int durationMs;
    uint8_t strength;
    bool crack;
  };

  uint8_t pin;
  bool enabled = true;
  Pending queue[THUNDER_QUEUE_SIZE];
  uint8_t pending = 0;
  Pending current;
  bool active = false;
  unsigned long startedAt = 0;
  unsigned long lastStepAt = 0;
};

#endif
