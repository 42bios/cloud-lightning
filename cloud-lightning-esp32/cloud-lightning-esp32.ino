/*
 * Copyright (c) 2015 Molly Nicholas
 * All rights reserved. See LICENSE for the full license text.
 *
 * Lightning cloud for an ESP32. Choose in config.h how it is controlled:
 * standalone with its own random storm (network_none.h), Home Assistant over
 * WiFi/MQTT (network_wifi.h) or Zigbee (network_zigbee.h). Several clouds
 * share one storm over Bluetooth LE (sync_ble.h), in any of these modes.
 * Optional: thunder sound and vibration (thunder.h).
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

#include "lightning.h"
#include "button.h"

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
CloudLightning lightning(strip);
Button button(BUTTON_PIN);

#if defined(ENABLE_THUNDER) || defined(ENABLE_RUMBLE)
#include "thunder.h"
#endif
#ifdef ENABLE_THUNDER
Thunder thunder(Serial1);
#endif
#ifdef ENABLE_RUMBLE
Rumble rumble(RUMBLE_PIN);
#endif

Preferences settings; // remembers ambient mode, sound and vibration across restarts

// Flash received from the leader, shown in loop().
Flash receivedFlash;
bool hasReceivedFlash = false;

// Commands and state, used by the network and sync code.
void commandStorm();
void commandStop();
void commandAmbient(bool on);
void commandSound(bool on);
void commandVibration(bool on);
void commandStrike(float km);
void commandFlash(const Flash &flash);
bool soundEnabled();
bool vibrationEnabled();

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
  if (CLOUD_LEADER) {
#ifdef CONNECTIVITY_NONE
    // Without Home Assistant, the random storm is on unless switched off.
    lightning.setAmbient(settings.getBool("ambient", true));
#else
    lightning.setAmbient(settings.getBool("ambient", false));
#endif
  }
#ifdef ENABLE_THUNDER
  Serial1.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  thunder.setEnabled(settings.getBool("sound", true));
#endif
#ifdef ENABLE_RUMBLE
  rumble.begin();
  rumble.setEnabled(settings.getBool("vibration", true));
#endif

  button.begin();
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

  Flash flash = {};
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

  // With several speakers, each cloud only thunders for the flashes that
  // start in it.
  bool thunderHere = flashed &&
                     (!THUNDER_OWN_FLASHES_ONLY || flash.origin == lightning.cloudPosition());
#ifdef ENABLE_THUNDER
  if (thunderHere) {
    thunder.strikeAt(flash.km, flash.strokes);
  }
  thunder.update();
#endif
#ifdef ENABLE_RUMBLE
  if (thunderHere) {
    rumble.strikeAt(flash.km, flash.strokes);  // only now and then, see thunder.h
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
//   leader:   click = thunderstorm, hold 2 s = ambient storm on/off,
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
        Flash test = {1.0f, 1, lightning.cloudPosition(), 0, 0, true, (uint32_t)esp_random()};
        lightning.render(test);
      }
      break;
    case BUTTON_HOLD_2S:
      if (CLOUD_LEADER) {
        commandAmbient(!lightning.ambient());
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

void commandStorm() {
  lightning.startStorm();
  networkPublishState();
}

void commandStop() {
  lightning.stop();
#ifdef ENABLE_THUNDER
  thunder.cancel();
#endif
#ifdef ENABLE_RUMBLE
  rumble.cancel();
#endif
  settings.putBool("ambient", false);
  networkPublishState();
}

void commandAmbient(bool on) {
  lightning.setAmbient(on);
  settings.putBool("ambient", on);
  networkPublishState();
}

void commandSound(bool on) {
#ifdef ENABLE_THUNDER
  thunder.setEnabled(on);
  settings.putBool("sound", on);
  networkPublishState();
#else
  (void)on;
#endif
}

void commandVibration(bool on) {
#ifdef ENABLE_RUMBLE
  rumble.setEnabled(on);
  settings.putBool("vibration", on);
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

bool soundEnabled() {
#ifdef ENABLE_THUNDER
  return thunder.isEnabled();
#else
  return false;
#endif
}

bool vibrationEnabled() {
#ifdef ENABLE_RUMBLE
  return rumble.isEnabled();
#else
  return false;
#endif
}
