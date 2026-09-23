/*
 * Copyright (c) 2015 Molly Nicholas
 * All rights reserved. See LICENSE for the full license text.
 *
 * Lightning cloud for an ESP32 with Home Assistant integration over MQTT.
 *
 * The cloud announces itself via MQTT discovery (buttons for storm and stop,
 * switches for ambient mode and thunder sound) and shows real lightning
 * strikes published to <DEVICE_ID>/strike as the distance in km, e.g. by the
 * Home Assistant automation in home-assistant/cloud_lightning.yaml.
 *
 * Needs the libraries "Adafruit NeoPixel" and "PubSubClient".
 * Copy config.example.h to config.h and fill in your values.
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "lightning.h"

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
CloudLightning lightning(strip);

#ifdef ENABLE_THUNDER
#include "thunder.h"
Thunder thunder(Serial2);
#endif

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Preferences settings; // remembers ambient mode and sound across restarts
unsigned long lastMqttAttempt = 0;
bool mqttAttempted = false;

String topic(const char *name) {
  return String(DEVICE_ID "/") + name;
}

void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  settings.begin("lightning", false);
  lightning.setAmbient(settings.getBool("ambient", false));
#ifdef ENABLE_THUNDER
  Serial2.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  thunder.setEnabled(settings.getBool("sound", true));
#endif

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(1024); // discovery messages are larger than the default 256 bytes
  mqtt.setCallback(onMessage);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected() &&
      (!mqttAttempted || millis() - lastMqttAttempt > 5000)) {
    mqttAttempted = true;
    lastMqttAttempt = millis();
    connectMqtt();
  }
  mqtt.loop();

  float km = lightning.update();
#ifdef ENABLE_THUNDER
  if (km >= 0) {
    thunder.strikeAt(km);
  }
  thunder.update();
#else
  (void)km;
#endif
}

void connectMqtt() {
  String status = topic("status");
  const char *user = strlen(MQTT_USER) > 0 ? MQTT_USER : nullptr;
  const char *password = strlen(MQTT_PASSWORD) > 0 ? MQTT_PASSWORD : nullptr;
  if (!mqtt.connect(DEVICE_ID, user, password, status.c_str(), 0, true, "offline")) {
    Serial.printf("MQTT connect failed, state %d\n", mqtt.state());
    return;
  }
  mqtt.subscribe(topic("strike").c_str());
  mqtt.subscribe(topic("+/set").c_str());
  publishDiscovery();
  publishState();
  mqtt.publish(status.c_str(), "online", true);
}

// Home Assistant MQTT discovery: the cloud appears as a device automatically.
void publishDiscovery() {
  announce("button", "storm", "Thunderstorm", "mdi:weather-lightning", false);
  announce("button", "stop", "Stop", "mdi:stop", false);
  announce("switch", "ambient", "Ambient storm", "mdi:weather-lightning-rainy", true);
#ifdef ENABLE_THUNDER
  announce("switch", "sound", "Thunder sound", "mdi:volume-high", true);
#endif
}

void announce(const char *component, const char *key, const char *name, const char *icon, bool hasState) {
  String payload = String("{\"name\":\"") + name + "\"" +
                   ",\"uniq_id\":\"" DEVICE_ID "_" + key + "\"" +
                   ",\"icon\":\"" + icon + "\"" +
                   ",\"cmd_t\":\"" + topic(key) + "/set\"" +
                   (hasState ? String(",\"stat_t\":\"") + topic(key) + "/state\"" : String()) +
                   ",\"avty_t\":\"" + topic("status") + "\"" +
                   ",\"dev\":{\"ids\":[\"" DEVICE_ID "\"],\"name\":\"" DEVICE_NAME "\"}}";
  String configTopic = String("homeassistant/") + component + "/" DEVICE_ID "/" + key + "/config";
  mqtt.publish(configTopic.c_str(), payload.c_str(), true);
}

void publishState() {
  mqtt.publish(topic("ambient/state").c_str(), lightning.ambient() ? "ON" : "OFF", true);
#ifdef ENABLE_THUNDER
  mqtt.publish(topic("sound/state").c_str(), thunder.isEnabled() ? "ON" : "OFF", true);
#endif
}

void saveSettings() {
  settings.putBool("ambient", lightning.ambient());
#ifdef ENABLE_THUNDER
  settings.putBool("sound", thunder.isEnabled());
#endif
}

void onMessage(char *topicName, byte *payload, unsigned int length) {
  String name(topicName);
  String value;
  value.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    value += (char)payload[i];
  }
  value.trim();

  if (name == topic("strike")) {
    // Distance of a real strike in km, e.g. "12.5".
    if (value.length() > 0 && value[0] >= '0' && value[0] <= '9') {
      lightning.strikeAt(value.toFloat());
    }
    return;
  }

  if (name == topic("storm/set")) {
    lightning.startStorm();
  } else if (name == topic("stop/set")) {
    lightning.stop();
#ifdef ENABLE_THUNDER
    thunder.cancel();
#endif
  } else if (name == topic("ambient/set")) {
    lightning.setAmbient(value == "ON");
#ifdef ENABLE_THUNDER
  } else if (name == topic("sound/set")) {
    thunder.setEnabled(value == "ON");
#endif
  } else {
    return;
  }
  saveSettings();
  publishState();
}
