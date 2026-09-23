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

// Optional (see thunder.h): thunder sound via a DFPlayer Mini MP3 module and
// a vibration motor on a PWM pin that now and then trembles with a strike
// right above. Uncomment what is connected. They can then be switched on and
// off at runtime with the "t" (sound) and "v" (vibration) commands.
// #define ENABLE_THUNDER
// #define ENABLE_RUMBLE

// More LEDs spread across the cloud make the flashes look more spatial.
const int NUM_LEDS = 4;
const int LED_PIN = 4;
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
CloudLightning lightning(strip);

#if defined(ENABLE_THUNDER) || defined(ENABLE_RUMBLE)
#include "thunder.h"
#endif

#ifdef ENABLE_THUNDER
#include <SoftwareSerial.h>

const int DFPLAYER_RX_PIN = 10; // Arduino RX <- DFPlayer TX
const int DFPLAYER_TX_PIN = 11; // Arduino TX -> DFPlayer RX (via 1k resistor)

SoftwareSerial dfSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
Thunder thunder(dfSerial);
#endif

#ifdef ENABLE_RUMBLE
const int RUMBLE_PIN = 5;       // PWM pin -> MOSFET/transistor -> motor
Rumble rumble(RUMBLE_PIN);
#endif

void setup() {
  // Setup the Serial connection to talk over Bluetooth
  Serial.begin(9600);
  Serial.setTimeout(50); // for reading the distance of "b" commands

  // Neopixel setup
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

#ifdef ENABLE_THUNDER
  dfSerial.begin(9600);
#endif
#ifdef ENABLE_RUMBLE
  rumble.begin();
#endif
}

void loop() {
  handleCommand(readFromBluetooth());

  Flash flash = {};
  bool flashed = lightning.update(flash);
#ifdef ENABLE_THUNDER
  if (flashed) {
    thunder.strikeAt(flash.km, flash.strokes);
  }
  thunder.update();
#endif
#ifdef ENABLE_RUMBLE
  if (flashed) {
    rumble.strikeAt(flash.km, flash.strokes);
  }
  rumble.update();
#endif
  (void)flashed;
}

/**
 * f      = start a short thunderstorm
 * a      = toggle ambient mode (endless distant storm)
 * t      = toggle thunder sound
 * v      = toggle vibration
 * s      = stop everything
 * b<km>  = real strike at the given distance, e.g. "b12.5"
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
    case 't':
#ifdef ENABLE_THUNDER
      thunder.setEnabled(!thunder.isEnabled());
      Serial.println(thunder.isEnabled() ? F("sound on") : F("sound off"));
#else
      Serial.println(F("sound not enabled in firmware"));
#endif
      break;
    case 'v':
#ifdef ENABLE_RUMBLE
      rumble.setEnabled(!rumble.isEnabled());
      Serial.println(rumble.isEnabled() ? F("vibration on") : F("vibration off"));
#else
      Serial.println(F("vibration not enabled in firmware"));
#endif
      break;
    case 's':
      lightning.stop();
#ifdef ENABLE_THUNDER
      thunder.cancel();
#endif
#ifdef ENABLE_RUMBLE
      rumble.cancel();
#endif
      Serial.println(F("stopped"));
      break;
    case 'b': {
      String distance = Serial.readStringUntil('\n');
      distance.trim();
      if (distance.length() > 0 && distance[0] >= '0' && distance[0] <= '9') {
        lightning.strikeAt(distance.toFloat());
      }
      break;
    }
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
