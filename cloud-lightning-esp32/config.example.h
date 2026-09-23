// Copy this file to config.h and fill in your values.
// config.h is ignored by git, so your passwords stay out of the repository.

// ---------------------------------------------------------------------------
// Connectivity: choose exactly one.
//
// NONE:   standalone. The cloud runs its own random storm and is controlled
//         with its button. Any ESP32, e.g. a small ESP32-C3.
// WiFi:   any ESP32 except the H2. Home Assistant via MQTT (discovery).
//         WiFi together with ENABLE_SYNC fills the default app partition to
//         over 90 %; if it does not fit, select Tools > Partition Scheme:
//         "Minimal SPIFFS" or "Huge APP".
// Zigbee: ESP32-C6 or ESP32-H2. Joins your existing Zigbee network (ZHA or
//         Zigbee2MQTT) as a router. In the Arduino IDE select
//         Tools > Zigbee mode: "Zigbee ZCZR (coordinator/router)" and
//         Tools > Partition Scheme: "Zigbee ZCZR 4MB with spiffs".
// ---------------------------------------------------------------------------
// #define CONNECTIVITY_NONE
#define CONNECTIVITY_WIFI
// #define CONNECTIVITY_ZIGBEE

#define DEVICE_NAME "Lightning cloud"

// WiFi only ------------------------------------------------------------------
#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-wifi-password"

// MQTT broker, e.g. the Mosquitto add-on of Home Assistant.
#define MQTT_HOST "homeassistant.local"
#define MQTT_PORT 1883
#define MQTT_USER "cloud-lightning"
#define MQTT_PASSWORD "your-mqtt-password"

// Used for the MQTT client id and topics (<DEVICE_ID>/...). Must be unique
// for every cloud.
#define DEVICE_ID "cloud-lightning"

// Several clouds (Bluetooth LE, works with every connectivity) ---------------
// Exactly one cloud is the leader: it decides the flashes, takes the commands
// and real strikes and broadcasts every flash over BLE. The others (followers)
// only listen, so they can be small boards with CONNECTIVITY_NONE.
// Followers pair automatically when switched on right next to the leader.
// #define ENABLE_SYNC
#define CLOUD_LEADER true   // false for the followers
// How close a follower has to be to pair (signal strength in dBm): -50 is a
// few cm with the antennas close together. Lower it (e.g. -60) if pairing
// does not start inside the case.
#define PAIRING_RSSI -50

// LEDs and button ------------------------------------------------------------
// More LEDs spread across the cloud make the flashes look more spatial.
#define NUM_LEDS 4
#define LED_PIN 4
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
#define BUTTON_PIN 0        // BOOT button
#else
#define BUTTON_PIN 9        // BOOT button (ESP32-C3, -C6, -H2)
#endif

// Thunder (optional) ---------------------------------------------------------
// Sound via a DFPlayer Mini MP3 module (see thunder.h for the sound files)
// and/or a vibration motor. Uncomment what is connected. Both can then be
// switched on and off at runtime.
// #define ENABLE_THUNDER
// #define ENABLE_RUMBLE
#if CONFIG_IDF_TARGET_ESP32
#define DFPLAYER_RX_PIN 16  // ESP32 RX <- DFPlayer TX
#define DFPLAYER_TX_PIN 17  // ESP32 TX -> DFPlayer RX (via 1k resistor)
#define RUMBLE_PIN 25       // PWM -> MOSFET -> vibration motor
#else
#define DFPLAYER_RX_PIN 6   // RX <- DFPlayer TX
#define DFPLAYER_TX_PIN 7   // TX -> DFPlayer RX (via 1k resistor)
#define RUMBLE_PIN 5        // PWM -> MOSFET -> vibration motor
#endif
// With several clouds that each have a speaker: true = every cloud only
// thunders for flashes that start in it, so the thunder comes from the right
// direction. false = this cloud thunders for every flash (one speaker setup).
#define THUNDER_OWN_FLASHES_ONLY false

// Music mode (optional, leader only) ----------------------------------------
// I2S microphone (INMP441 or similar, L/R pin to GND): the cloud flashes on
// the beat, without thunder. Switch it on with a double click or in Home
// Assistant.
// #define ENABLE_MICROPHONE
#if CONFIG_IDF_TARGET_ESP32
#define MIC_SCK_PIN 26
#define MIC_WS_PIN 27
#define MIC_SD_PIN 33
#else
#define MIC_SCK_PIN 1
#define MIC_WS_PIN 3
#define MIC_SD_PIN 0
#endif
// Beats quieter than this are ignored. Raise it if the cloud flashes in a
// quiet room, lower it if it misses quiet music.
#define MIC_NOISE_FLOOR 20000
