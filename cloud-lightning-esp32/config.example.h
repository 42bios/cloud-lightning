// Copy this file to config.h and fill in your values.
// config.h is ignored by git, so your passwords stay out of the repository.

#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-wifi-password"

// MQTT broker, e.g. the Mosquitto add-on of Home Assistant.
#define MQTT_HOST "homeassistant.local"
#define MQTT_PORT 1883
#define MQTT_USER "cloud-lightning"
#define MQTT_PASSWORD "your-mqtt-password"

// Used for the MQTT client id, the topics (cloud-lightning/...) and in Home Assistant.
#define DEVICE_ID "cloud-lightning"
#define DEVICE_NAME "Lightning cloud"

// More LEDs spread across the cloud make the flashes look more spatial.
#define NUM_LEDS 4
#define LED_PIN 4

// Optional thunder sound via a DFPlayer Mini MP3 module (see thunder.h).
// Uncomment when the module is connected. It then shows up as a switch in
// Home Assistant.
// #define ENABLE_THUNDER
#define DFPLAYER_RX_PIN 16 // ESP32 RX <- DFPlayer TX
#define DFPLAYER_TX_PIN 17 // ESP32 TX -> DFPlayer RX (via 1k resistor)
