/*
 * Copyright (c) 2015 Molly Nicholas
 * All rights reserved. See LICENSE for the full license text.
 *
 * Lightning cloud for an ESP32. Choose in config.h how it is controlled:
 * standalone with its own random storm (network_none.h), Home Assistant over
 * WiFi/MQTT (network_wifi.h) or Zigbee (network_zigbee.h). Several clouds
 * share one storm over Bluetooth LE (sync_ble.h), in any of these modes.
 * Optional: thunder sound and vibration (thunder.h), music mode with a
 * microphone (microphone.h).
 *
 * Needs the library "Adafruit NeoPixel", for WiFi also "PubSubClient", for
 * several clouds also "NimBLE-Arduino".
 * Copy config.example.h to config.h and fill in your values.
 */

#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

#if defined(CONNECTIVITY_NONE) + defined(CONNECTIVITY_WIFI) + defined(CONNECTIVITY_ZIGBEE) != 1
#error "config.h: choose exactly one of CONNECTIVITY_NONE, CONNECTIVITY_WIFI, CONNECTIVITY_ZIGBEE"
#endif

#if !CLOUD_LEADER && !defined(ENABLE_SYNC)
#error "config.h: a follower cloud (CLOUD_LEADER false) needs ENABLE_SYNC"
#endif

#if defined(ENABLE_THUNDER) || defined(ENABLE_RUMBLE)
#define HAS_THUNDER
#endif

#include "lightning.h"
#include "button.h"

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
CloudLightning lightning(strip);
Button button(BUTTON_PIN);

#ifdef HAS_THUNDER
#include "thunder.h"
#endif
#ifdef ENABLE_THUNDER
Thunder thunder(Serial1);
#endif
#ifdef ENABLE_RUMBLE
Rumble rumble(RUMBLE_PIN);
#endif
#ifdef ENABLE_MICROPHONE
#include "microphone.h"
BeatDetector microphone;
#endif

Preferences settings;        // remembers ambient mode and thunder across restarts
bool ambientWanted = false;  // ambient setting; paused while in music mode
bool thunderOn = true;
bool musicOn = false;

// Flash received from the leader, shown in loop().
Flash receivedFlash;
bool hasReceivedFlash = false;

// Commands and state, used by the network and sync code.
void commandStorm();
void commandStop();
void commandAmbient(bool on);
void commandThunder(bool on);
void commandMusic(bool on);
void commandStrike(float km);
void commandFlash(const Flash &flash);
bool ambientEnabled();
bool thunderEnabled();
bool musicEnabled();

#if defined(CONNECTIVITY_WIFI)
#include "network_wifi.h"
#elif defined(CONNECTIVITY_ZIGBEE)
#include "network_zigbee.h"
#else
#include "network_none.h"
#endif

#ifdef ENABLE_SYNC
#include "sync_ble.h"
#endif

void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  settings.begin("lightning", false);
#ifdef CONNECTIVITY_NONE
  // Without Home Assistant, the random storm is on unless switched off.
  ambientWanted = settings.getBool("ambient", true);
#else
  ambientWanted = settings.getBool("ambient", false);
#endif
  if (CLOUD_LEADER) {
    lightning.setAmbient(ambientWanted);
  }
  thunderOn = settings.getBool("thunder", true);
#ifdef ENABLE_THUNDER
  Serial1.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  thunder.setEnabled(thunderOn);
#endif
#ifdef ENABLE_RUMBLE
  rumble.begin();
  rumble.setEnabled(thunderOn);
#endif

#ifdef ENABLE_MICROPHONE
  button.begin(CLOUD_LEADER);  // double click switches music mode
#else
  button.begin(false);
#endif
  networkSetup();
#ifdef ENABLE_SYNC
  syncSetup();
#endif
}

void loop() {
  networkLoop();
  handleButton();
#ifdef ENABLE_SYNC
  syncLoop();
#endif
#ifdef ENABLE_MICROPHONE
  if (musicOn) {
    float km = microphone.update();
    if (km >= 0) {
      lightning.pulseAt(km);
    }
  }
#endif

  Flash flash;
  bool flashed = false;
  if (CLOUD_LEADER) {
    if (lightning.poll(flash)) {
#ifdef ENABLE_SYNC
      syncSendFlash(flash);
#endif
      lightning.render(flash);
      lightning.finished();
      flashed = true;
    }
  } else if (hasReceivedFlash) {
    flash = receivedFlash;
    hasReceivedFlash = false;
    lightning.render(flash);
    flashed = true;
  }

  // No thunder in music mode, and with several speakers each cloud only
  // thunders for the flashes that start in it.
  bool thunderHere = flashed && !flash.quick && !musicOn &&
                     (!THUNDER_OWN_FLASHES_ONLY || flash.origin == lightning.cloudPosition());
#ifdef ENABLE_THUNDER
  if (thunderHere) {
    thunder.strikeAt(flash.km, flash.strokes);
  }
  thunder.update();
#endif
#ifdef ENABLE_RUMBLE
  if (thunderHere) {
    rumble.strikeAt(flash.km, flash.strokes);
  }
  rumble.update();
#endif
  (void)thunderHere;

  // Let Home Assistant know when a thunderstorm has ended.
  static bool stormWasActive = false;
  if (lightning.stormActive() != stormWasActive) {
    stormWasActive = lightning.stormActive();
    networkPublishState();
  }
}

// Button:
//   leader:   click = thunderstorm, double click = music mode,
//             hold 2 s = ambient storm on/off,
//             hold 6 s = new installation (forget the paired clouds),
//             hold 10 s = leave the Zigbee network
//   follower: click = test flash, hold 6 s = forget the leader, pair again
// While the button is held, the cloud blinks once at 2, 6 and 10 s.
void handleButton() {
  static uint8_t shownLevel = 0;
  uint8_t level = button.holdLevel();
  if (level > shownLevel) {
    lightning.fill(60, 60, 80);
    delay(80);
    lightning.off();
  }
  shownLevel = level;

  switch (button.update()) {
    case BUTTON_CLICK:
      if (CLOUD_LEADER) {
        commandStorm();
      } else {
        Flash test = {1.0f, 1, lightning.cloudPosition(), 0, 0, true, true, (uint32_t)esp_random()};
        lightning.render(test);
      }
      break;
    case BUTTON_DOUBLE_CLICK:
      commandMusic(!musicOn);
      break;
    case BUTTON_HOLD_2S:
      if (CLOUD_LEADER) {
        commandAmbient(!ambientWanted);
      }
      break;
    case BUTTON_HOLD_6S:
#ifdef ENABLE_SYNC
      syncReset();
#endif
      break;
    case BUTTON_HOLD_10S:
      networkReset();
      break;
    case BUTTON_NONE:
      break;
  }
}

void saveSettings() {
  settings.putBool("ambient", ambientWanted);
  settings.putBool("thunder", thunderOn);
}

void cancelThunder() {
#ifdef ENABLE_THUNDER
  thunder.cancel();
#endif
#ifdef ENABLE_RUMBLE
  rumble.cancel();
#endif
}

void commandStorm() {
  lightning.startStorm();
  networkPublishState();
}

void commandStop() {
  lightning.stop();
  ambientWanted = false;
  musicOn = false;
  cancelThunder();
  saveSettings();
  networkPublishState();
}

void commandAmbient(bool on) {
  ambientWanted = on;
  if (!musicOn) {
    lightning.setAmbient(on);
  }
  saveSettings();
  networkPublishState();
}

void commandThunder(bool on) {
  thunderOn = on;
#ifdef ENABLE_THUNDER
  thunder.setEnabled(on);
#endif
#ifdef ENABLE_RUMBLE
  rumble.setEnabled(on);
#endif
  saveSettings();
  networkPublishState();
}

// Music mode: flashes on the beat instead of the storm, and no thunder: the
// speaker would trigger the microphone, and thunder does not go with music.
void commandMusic(bool on) {
#ifdef ENABLE_MICROPHONE
  if (on && !microphone.begin()) {
    Serial.println("Microphone failed to start");
    on = false;
  }
  musicOn = on;
  lightning.setAmbient(on ? false : ambientWanted);
  cancelThunder();
  networkPublishState();
#else
  (void)on;
#endif
}

void commandStrike(float km) {
  lightning.strikeAt(km);
}

void commandFlash(const Flash &flash) {
  receivedFlash = flash;
  hasReceivedFlash = true;
}

bool ambientEnabled() {
  return ambientWanted;
}

bool thunderEnabled() {
  return thunderOn;
}

bool musicEnabled() {
  return musicOn;
}
