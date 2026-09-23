/*
 * Copyright (c) 2015 Molly Nicholas
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * (subject to the limitations in the disclaimer below) provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions
 * and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 * and the following disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of Molly Nicholas nor the names of its contributors may be used to
 * endorse or promote products derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE. THIS
 * SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Adafruit_NeoPixel.h>
#include "lightning.h"

// Optional thunder sound via a DFPlayer Mini MP3 module.
// Needs the "DFRobotDFPlayerMini" library and thunder MP3s on the SD card
// (0001.mp3 ... 000N.mp3 in the root or in /mp3). Uncomment to enable.
// #define ENABLE_THUNDER

// More LEDs spread across the cloud make the flashes look more spatial.
const int NUM_LEDS = 4;
const int LED_PIN = 4;
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
CloudLightning lightning(strip);

#ifdef ENABLE_THUNDER
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

const int DFPLAYER_RX_PIN = 10; // Arduino RX <- DFPlayer TX
const int DFPLAYER_TX_PIN = 11; // Arduino TX -> DFPlayer RX (via 1k resistor)
const int THUNDER_TRACKS = 3;

SoftwareSerial dfSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
DFRobotDFPlayerMini dfPlayer;
bool dfPlayerReady = false;
unsigned long thunderAt = 0;
uint8_t thunderVolume = 0; // 0 = no thunder pending
#endif

void setup() {
  // Setup the Serial connection to talk over Bluetooth
  Serial.begin(9600);

  // Neopixel setup
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

#ifdef ENABLE_THUNDER
  dfSerial.begin(9600);
  dfPlayerReady = dfPlayer.begin(dfSerial);
#endif
}

void loop() {
  handleCommand(readFromBluetooth());

  uint8_t peak = lightning.update();
  if (peak > 0) {
    scheduleThunder(peak);
  }
  playPendingThunder();
}

/**
 * f = start a short thunderstorm
 * a = toggle ambient mode (endless distant storm)
 * s = stop everything
 */
void handleCommand(char command) {
  switch (command) {
    case 'f':
      lightning.startStorm();
      break;
    case 'a':
      lightning.setAmbient(!lightning.ambient());
      Serial.println(lightning.ambient() ? F("ambient on") : F("ambient off"));
      break;
    case 's':
      lightning.stop();
      Serial.println(F("stopped"));
      break;
  }
}

/**
 * Read a single command byte from BLE, skipping line endings.
 */
char readFromBluetooth() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      continue;
    }
    return c;
  }
  return '\0';
}

#ifdef ENABLE_THUNDER
// Sound is much slower than light: close (bright) strikes rumble soon and
// loud, farther ones later and quieter, distant sheet lightning stays silent.
void scheduleThunder(uint8_t peak) {
  if (!dfPlayerReady || thunderVolume != 0 || peak < CLOSE_STRIKE_PEAK) {
    return;
  }
  thunderAt = millis() + map(peak, CLOSE_STRIKE_PEAK, 255, 4000, 300) + random(0, 300);
  thunderVolume = map(peak, CLOSE_STRIKE_PEAK, 255, 12, 30);
}

void playPendingThunder() {
  if (thunderVolume == 0 || (long)(millis() - thunderAt) < 0) {
    return;
  }
  dfPlayer.volume(thunderVolume);
  dfPlayer.play(random(1, THUNDER_TRACKS + 1));
  thunderVolume = 0;
}
#else
void scheduleThunder(uint8_t) {}
void playPendingThunder() {}
#endif
