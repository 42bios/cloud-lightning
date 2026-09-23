/*
 * Zigbee connectivity (ESP32-C6 / ESP32-H2), e.g. with ZHA or Zigbee2MQTT.
 *
 * The cloud joins as a router (it is mains powered, so it also strengthens
 * your Zigbee mesh) and offers these endpoints:
 *   10  on/off light   Thunderstorm (on = start; turns off when it has ended)
 *   11  on/off light   Ambient storm
 *   12  analog output  Real strike: write the distance in km
 *   13  on/off light   Thunder, sound and vibration (only with thunder hardware)
 *   14  on/off light   Music mode (only with ENABLE_MICROPHONE)
 *
 * The cloud starts pairing automatically when it is not in a network yet.
 * Hold the button for 10 s to leave the network and pair again.
 */

#include "Zigbee.h"

#if !defined(ZIGBEE_MODE_ED) && !defined(ZIGBEE_MODE_ZCZR)
#error "Zigbee: select Tools > Zigbee mode \"Zigbee ZCZR (coordinator/router)\" and a Zigbee partition scheme"
#endif

ZigbeeLight zbStorm(10);
ZigbeeLight zbAmbient(11);
ZigbeeAnalog zbStrike(12);
#ifdef HAS_THUNDER
ZigbeeLight zbSound(13);
#endif
#ifdef ENABLE_MICROPHONE
ZigbeeLight zbMusic(14);
#endif

// The Zigbee callbacks run in the Zigbee task. They only queue the request;
// loop() carries it out, so the LEDs are never driven from two tasks at once.
// setLight() also calls the callbacks; our own state updates are ignored.
enum ZbRequestType : uint8_t { ZB_STORM, ZB_AMBIENT, ZB_SOUND, ZB_MUSIC, ZB_STRIKE };
struct ZbRequest {
  ZbRequestType type;
  bool on;
  float km;
};
QueueHandle_t zbRequests;
volatile bool zbPublishing = false;
bool zbWasConnected = false;

void zbQueue(ZbRequestType type, bool on, float km) {
  if (zbPublishing) {
    return;
  }
  ZbRequest request = {type, on, km};
  xQueueSend(zbRequests, &request, 0);
}

void onZbStorm(bool on) {
  zbQueue(ZB_STORM, on, 0);
}

void onZbAmbient(bool on) {
  zbQueue(ZB_AMBIENT, on, 0);
}

void onZbSound(bool on) {
  zbQueue(ZB_SOUND, on, 0);
}

void onZbMusic(bool on) {
  zbQueue(ZB_MUSIC, on, 0);
}

void onZbStrike(float km) {
  zbQueue(ZB_STRIKE, false, km);
}

void networkPublishState() {
  if (!Zigbee.connected()) {
    return;
  }
  zbPublishing = true;
  zbStorm.setLight(lightning.stormActive());
  zbAmbient.setLight(ambientEnabled());
#ifdef HAS_THUNDER
  zbSound.setLight(thunderEnabled());
#endif
#ifdef ENABLE_MICROPHONE
  zbMusic.setLight(musicEnabled());
#endif
  zbPublishing = false;
}

void networkSetup() {
  zbRequests = xQueueCreate(8, sizeof(ZbRequest));

  zbStorm.setManufacturerAndModel("cloud-lightning", DEVICE_NAME);
  zbStorm.onLightChange(onZbStorm);
  zbAmbient.onLightChange(onZbAmbient);
  zbStrike.addAnalogOutput();
  zbStrike.onAnalogOutputChange(onZbStrike);

  Zigbee.addEndpoint(&zbStorm);
  Zigbee.addEndpoint(&zbAmbient);
  Zigbee.addEndpoint(&zbStrike);
#ifdef HAS_THUNDER
  zbSound.onLightChange(onZbSound);
  Zigbee.addEndpoint(&zbSound);
#endif
#ifdef ENABLE_MICROPHONE
  zbMusic.onLightChange(onZbMusic);
  Zigbee.addEndpoint(&zbMusic);
#endif

#ifdef ZIGBEE_MODE_ZCZR
  bool started = Zigbee.begin(ZIGBEE_ROUTER);
#else
  bool started = Zigbee.begin();
#endif
  if (!started) {
    Serial.println("Zigbee failed to start, restarting");
    ESP.restart();
  }
}

void networkLoop() {
  ZbRequest request;
  while (xQueueReceive(zbRequests, &request, 0) == pdTRUE) {
    switch (request.type) {
      case ZB_STORM:
        if (request.on) {
          commandStorm();
        } else {
          commandStop();
        }
        break;
      case ZB_AMBIENT:
        commandAmbient(request.on);
        break;
      case ZB_SOUND:
        commandThunder(request.on);
        break;
      case ZB_MUSIC:
        commandMusic(request.on);
        break;
      case ZB_STRIKE:
        commandStrike(request.km);
        break;
    }
  }

  // Report the current state once after joining the network.
  if (Zigbee.connected() != zbWasConnected) {
    zbWasConnected = Zigbee.connected();
    networkPublishState();
  }
}

// Leave the Zigbee network and start pairing again (restarts the board).
void networkReset() {
  Zigbee.factoryReset();
}
