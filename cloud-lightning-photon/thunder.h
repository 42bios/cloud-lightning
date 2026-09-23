/*
 * Copyright (c) 2015 Molly Nicholas
 * All rights reserved. See LICENSE for the full license text.
 *
 * Optional thunder sound via a DFPlayer Mini MP3 module.
 *
 * Shared by cloud-lightning (Arduino) and cloud-lightning-photon (Particle).
 * Both copies of this file must stay identical.
 *
 * Thunder follows the flash with the real delay of sound (about 3 s per km)
 * and gets quieter with distance; beyond THUNDER_MAX_KM it stays silent.
 * Put the sounds on the SD card as /mp3/0001.mp3, /mp3/0002.mp3, ...
 */

#ifndef CLOUD_THUNDER_H
#define CLOUD_THUNDER_H

const float SPEED_OF_SOUND_KM_PER_S = 0.343;
const float THUNDER_MAX_KM = 15;   // thunder is rarely audible beyond this
const uint8_t THUNDER_TRACKS = 3;  // number of MP3 files in /mp3
const uint8_t THUNDER_QUEUE_SIZE = 4;

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
  }

  // Schedules the thunder for a flash at the given distance.
  void strikeAt(float km) {
    if (!enabled || km < 0 || km > THUNDER_MAX_KM || pending >= THUNDER_QUEUE_SIZE) {
      return;
    }
    queue[pending].at = millis() + (unsigned long)(km / SPEED_OF_SOUND_KM_PER_S * 1000);
    queue[pending].volume = 30 - (uint8_t)(km / THUNDER_MAX_KM * 20);  // 30 close, 10 far
    pending++;
  }

  // Call from loop(). Plays thunder that is due.
  void update() {
    for (uint8_t i = 0; i < pending; i++) {
      if ((long)(millis() - queue[i].at) >= 0) {
        send(0x06, queue[i].volume);             // set volume (0-30)
        delay(30);                               // DFPlayer needs a short gap between commands
        send(0x12, random(1, THUNDER_TRACKS + 1)); // play /mp3/000N.mp3
        queue[i] = queue[--pending];
        return;
      }
    }
  }

private:
  struct Pending {
    unsigned long at;
    uint8_t volume;
  };

  Stream &player;
  bool enabled = true;
  Pending queue[THUNDER_QUEUE_SIZE];
  uint8_t pending = 0;

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

#endif
