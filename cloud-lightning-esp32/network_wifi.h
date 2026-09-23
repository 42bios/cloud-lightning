/*
 * WiFi + MQTT connectivity with Home Assistant MQTT discovery.
 *
 * The leader cloud announces buttons for thunderstorm and stop and switches
 * for ambient storm, thunder (sound and vibration) and music mode; follower
 * clouds (see sync_ble.h) only the thunder switch. Real strikes are published
 * to <DEVICE_ID>/strike as the distance in km.
 */

#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long lastMqttAttempt = 0;
bool mqttAttempted = false;

String topic(const char *name) {
  return String(DEVICE_ID "/") + name;
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

// Home Assistant MQTT discovery: the cloud appears as a device automatically.
void publishDiscovery() {
  if (CLOUD_LEADER) {
    announce("button", "storm", "Thunderstorm", "mdi:weather-lightning", false);
    announce("button", "stop", "Stop", "mdi:stop", false);
    announce("switch", "ambient", "Ambient storm", "mdi:weather-lightning-rainy", true);
#ifdef ENABLE_MICROPHONE
    announce("switch", "music", "Music mode", "mdi:music", true);
#endif
  }
#ifdef HAS_THUNDER
  announce("switch", "sound", "Thunder", "mdi:volume-high", true);
#endif
}

void networkPublishState() {
  if (!mqtt.connected()) {
    return;
  }
  if (CLOUD_LEADER) {
    mqtt.publish(topic("ambient/state").c_str(), ambientEnabled() ? "ON" : "OFF", true);
#ifdef ENABLE_MICROPHONE
    mqtt.publish(topic("music/state").c_str(), musicEnabled() ? "ON" : "OFF", true);
#endif
  }
#ifdef HAS_THUNDER
  mqtt.publish(topic("sound/state").c_str(), thunderEnabled() ? "ON" : "OFF", true);
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
      commandStrike(value.toFloat());
    }
  } else if (name == topic("storm/set")) {
    commandStorm();
  } else if (name == topic("stop/set")) {
    commandStop();
  } else if (name == topic("ambient/set")) {
    commandAmbient(value == "ON");
  } else if (name == topic("sound/set")) {
    commandThunder(value == "ON");
  } else if (name == topic("music/set")) {
    commandMusic(value == "ON");
  }
}

void connectMqtt() {
  String status = topic("status");
  const char *user = strlen(MQTT_USER) > 0 ? MQTT_USER : nullptr;
  const char *password = strlen(MQTT_PASSWORD) > 0 ? MQTT_PASSWORD : nullptr;
  if (!mqtt.connect(DEVICE_ID, user, password, status.c_str(), 0, true, "offline")) {
    Serial.printf("MQTT connect failed, state %d\n", mqtt.state());
    return;
  }
  if (CLOUD_LEADER) {
    mqtt.subscribe(topic("strike").c_str());
  }
  mqtt.subscribe(topic("+/set").c_str());
  publishDiscovery();
  networkPublishState();
  mqtt.publish(status.c_str(), "online", true);
}

void networkSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(1024); // discovery messages are larger than the default 256 bytes
  mqtt.setCallback(onMessage);
}

void networkReset() {
}

void networkLoop() {
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected() &&
      (!mqttAttempted || millis() - lastMqttAttempt > 5000)) {
    mqttAttempted = true;
    lastMqttAttempt = millis();
    connectMqtt();
  }
  mqtt.loop();
}
