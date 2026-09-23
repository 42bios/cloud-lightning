/*
 * Copyright (c) 2015 Molly Nicholas
 * All rights reserved. See LICENSE for the full license text.
 *
 * Lightning cloud for an ESP32. Choose in config.h how it is controlled:
 * standalone with its own random storm (network_none.h), Home Assistant over
 * WiFi/MQTT (network_wifi.h) or Zigbee (network_zigbee.h). Several clouds
 * share one storm over Bluetooth LE (sync_ble.h), in any of these modes.
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

#include "lightning.h"

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
CloudLightning lightning(strip, CLOUD_COUNT, CLOUD_POSITION);

#ifdef ENABLE_THUNDER
#include "thunder.h"
Thunder thunder(Serial1);
#endif

Preferences settings; // remembers ambient mode and sound across restarts

// Commands, called by the network code.
void commandStorm();
void commandStop();
void commandAmbient(bool on);
void commandSound(bool on);
void commandStrike(float km);
bool soundEnabled();

#if defined(CONNECTIVITY_WIFI)
#include "network_wifi.h"
#elif defined(CONNECTIVITY_ZIGBEE)
#include "network_zigbee.h"
#else
#include "network_none.h"
#endif

#if CLOUD_COUNT > 1
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

  networkSetup();
#if CLOUD_COUNT > 1
  syncSetup();
#endif
}

void loop() {
  networkLoop();

  Flash flash;
  bool flashed = false;
  if (CLOUD_LEADER) {
    if (lightning.poll(flash)) {
#if CLOUD_COUNT > 1
      syncSendFlash(flash);
#endif
      lightning.render(flash);
      lightning.finished();
      flashed = true;
    }
  } else {
#if CLOUD_COUNT > 1
    if (syncReceive(flash)) {
      lightning.render(flash);
      flashed = true;
    }
#endif
  }

#ifdef ENABLE_THUNDER
  if (flashed && (!THUNDER_OWN_FLASHES_ONLY || flash.origin == CLOUD_POSITION)) {
    thunder.strikeAt(flash.km, flash.strokes);
  }
  thunder.update();
#else
  (void)flashed;
#endif

  // Let Home Assistant know when a thunderstorm has ended.
  static bool stormWasActive = false;
  if (lightning.stormActive() != stormWasActive) {
    stormWasActive = lightning.stormActive();
    networkPublishState();
  }
}

void saveSettings() {
  settings.putBool("ambient", lightning.ambient());
#ifdef ENABLE_THUNDER
  settings.putBool("sound", thunder.isEnabled());
#endif
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
  saveSettings();
  networkPublishState();
}

void commandAmbient(bool on) {
  lightning.setAmbient(on);
  saveSettings();
  networkPublishState();
}

void commandSound(bool on) {
#ifdef ENABLE_THUNDER
  thunder.setEnabled(on);
  saveSettings();
  networkPublishState();
#else
  (void)on;
#endif
}

bool soundEnabled() {
#ifdef ENABLE_THUNDER
  return thunder.isEnabled();
#else
  return false;
#endif
}

void commandStrike(float km) {
  lightning.strikeAt(km);
}
