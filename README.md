# cloud-lightning

[![Lightning cloud](http://i.imgur.com/79LUuV7.png)](https://www.youtube.com/watch?v=XxMMNcU-hWE "Click to see the video")

A little lightning cloud for fun. [Full Instructable available here.](http://www.instructables.com/id/How-to-make-a-Lightning-Cloud/)

## Security note
Do not commit Particle device IDs, access tokens, or any other secrets to this repository.
The sample web controller now asks for credentials at runtime instead of storing them in source.

## V2.0 – natural lightning

The flash animation was rewritten to follow how real lightning behaves:

- **Stepped leader:** a few faint flickers before the main flash.
- **Return strokes:** one bright main stroke followed by 1–4 weaker subsequent strokes, 30–120 ms apart.
- **Afterglow:** every stroke fades out exponentially instead of switching off hard.
- **Gamma correction:** brightness is corrected for the non-linear perception of the eye, so fades look smooth.
- **Colour:** slightly blue-white instead of pure white.
- **Light spreads inside the cloud:** neighbouring LEDs glow along, and the channel sometimes wanders to the next LED.
- **Distance:** every flash has a distance. Close strikes are bright with several strokes, distant ones are faint, diffuse sheet lightning.
- **Realistic timing:** pauses between flashes are exponentially distributed (mostly short, occasionally long).
- **Ambient mode:** an endless distant storm, mostly sheet lightning with an occasional close strike.

All of the following is optional:

- **Thunder:** follows the flash with the real delay of sound (about 3 s per km). It cracks sharply when close and only rumbles when far, lasts longer for distant strikes and flashes with more strokes, gets quieter with distance and stays silent beyond 15 km. Can be switched on and off at runtime.
- **Several clouds:** an installation with e.g. 3 clouds shares one storm over Bluetooth LE, without WiFi or Home Assistant. The clouds don't just show the same: a flash stays inside one cloud, jumps on from cloud to cloud, or lights the others faintly from the side, and each cloud flickers in its own way.
- **Home Assistant:** over WiFi (MQTT) or Zigbee, chosen in the configuration. Without it, the cloud runs its own random storm.
- **Real lightning:** through Home Assistant the cloud shows real strikes around your location, reported by [Blitzortung.org](https://www.blitzortung.org).

It also fixes some problems of the old version:

- The old "moving average" brightness was really a random walk that drifted, so after a while almost every flash was at full brightness.
- Pixels were not switched off between the flashes of a burst.
- The Arduino loop waited 1 s between reads, so BLE commands could take up to a second to react. The loop is now non-blocking between flashes.

The Particle Photon version was removed because the Photon has reached end of life. It is still in the git history (commit `85856e3`).

### Which version?

| Folder | Hardware | Control |
| --- | --- | --- |
| `cloud-lightning/` | Arduino + Bluefruit BLE module | BLE app |
| `cloud-lightning-esp32/` | ESP32, ESP32-C3, ESP32-C6 | standalone, Home Assistant over WiFi or Zigbee |

The ESP32 version is set up in `config.h`, choosing exactly one connectivity:

- **None:** standalone. The cloud runs its own random storm. The BOOT button starts a thunderstorm (short press) or switches the random storm on and off (hold 2 s).
- **WiFi:** Home Assistant with an MQTT broker (e.g. the Mosquitto add-on). The cloud appears automatically via MQTT discovery.
- **Zigbee** (ESP32-C6 or -H2): your existing Zigbee coordinator (ZHA or Zigbee2MQTT). The cloud joins as a router, so it also strengthens your Zigbee mesh.

No base station is needed in any mode. Recommended boards: an **ESP32-C6** for the main cloud (it can do WiFi and Zigbee, so you can switch without new hardware) and a small **ESP32-C3** (e.g. SuperMini) for additional clouds that only listen over BLE.

**Power:** a cloud with a network connection is meant to be mains powered with a 5 V USB power supply (2 A with thunder, 1 A without), with the cable running along the cord the cloud hangs from. WiFi and a Zigbee router stay awake all the time and would empty a battery within about a day. Follower clouds that only listen over BLE are smaller and simpler, but listening all the time still takes about 80–100 mA: an ESP32-C3 with a 2000 mAh battery lasts roughly a day. That is enough for an evening or a party, but not for permanent use.

### Commands (Arduino + BLE)

| Command | Effect |
| --- | --- |
| `f` | Start a short thunderstorm (3–6 flashes) |
| `a` | Toggle ambient mode (endless distant storm) |
| `t` | Toggle thunder sound |
| `s` | Stop everything |
| `b<km>` | Show a real strike at the given distance, e.g. `b12.5` |

Send the commands over BLE, for example with the Adafruit Bluefruit app.

### ESP32 in Home Assistant

**WiFi:** the cloud shows up as a device with the buttons *Thunderstorm* and *Stop* and the switches *Ambient storm* and *Thunder sound*. Real strikes are published to `<DEVICE_ID>/strike` as the distance in km.

**Zigbee:** the cloud offers four endpoints: on/off lights for *Thunderstorm* (turns off when the storm has ended), *Ambient storm* and *Thunder sound*, and an analog output (a number in Home Assistant) that shows a real strike at the written distance in km. Rename the entities in Home Assistant as you like. It pairs automatically when it is not in a network yet. Hold the BOOT button for 3 s to leave the network and pair again. With ZHA the endpoints work directly. Zigbee2MQTT may need an external converter for the analog output.

In both modes, ambient mode and sound are remembered across restarts.

### Several clouds (Bluetooth LE)

The clouds of an installation share one storm over Bluetooth LE. This works with every connectivity, also without WiFi and Home Assistant.

1. Give all clouds the same `SYNC_GROUP` and `CLOUD_COUNT` (and, with WiFi, each its own `DEVICE_ID`).
2. Number them with `CLOUD_POSITION` from left to right (0, 1, 2 …).
3. Set `CLOUD_LEADER true` on exactly one cloud. It runs the storm, receives the commands and real strikes, and broadcasts every flash. The others only listen, so they can be small ESP32-C3 boards with `CONNECTIVITY_NONE`.

The clouds don't simply show the same flash. Each flash starts in one cloud and is one of three kinds:

- **Intra-cloud flash:** it stays inside that cloud.
- **Jump:** the channel moves on to the neighbouring clouds in one direction, a moment later and weaker in each.
- **Big strike:** it lights the other clouds faintly from the side, where not every stroke is visible.

Distant sheet lightning lights all clouds faintly. The rhythm of the strokes is the same everywhere because it is one flash. Where it flickers and how bright is different in every cloud.

With thunder, either one cloud has a speaker and plays every thunder, or several clouds have speakers and each plays only the thunder of flashes that start in it (`THUNDER_OWN_FLASHES_ONLY true`). Then the thunder comes from the right direction.

BLE together with WiFi works on all ESP32 boards. BLE together with Zigbee on the ESP32-C6 compiles, but has not been tested on hardware yet.

### Configuration

- Arduino: `NUM_LEDS`, `LED_PIN` and `ENABLE_THUNDER` in `cloud-lightning.ino`.
- ESP32: copy `config.example.h` to `config.h` and fill in connectivity, WiFi/MQTT, clouds, LEDs and thunder. `config.h` is ignored by git. Libraries: `Adafruit NeoPixel`; with WiFi also `PubSubClient`; with several clouds also `NimBLE-Arduino` (2.x).
- `lightning.h`: colour (`LIGHTNING_R/G/B`), storm pace (`STORM_MEAN_PAUSE_MS`, `AMBIENT_MEAN_PAUSE_MS`) and how distance changes a flash (`CLOSE_STRIKE_KM`, `MAX_STRIKE_KM`).
- `thunder.h`: number of sound files per folder (`THUNDER_TRACKS`) and thunder length (`THUNDER_MIN_MS`, `THUNDER_MAX_MS`).
- `lightning.h` and `thunder.h` are shared. The copies in both sketch folders must stay identical, because Arduino only compiles files inside the sketch folder. The GitHub workflow checks this and compiles all variants.

### Thunder (optional)

Uses a DFPlayer Mini MP3 module. No extra library is needed.

1. Wire the DFPlayer Mini: power, a speaker, and
   - Arduino: DFPlayer TX → pin 10, DFPlayer RX → pin 11 (through a 1 kΩ resistor)
   - ESP32: DFPlayer TX → GPIO 16, DFPlayer RX → GPIO 17 (through a 1 kΩ resistor)
   - ESP32-C3 / -C6: DFPlayer TX → GPIO 6, DFPlayer RX → GPIO 7 (through a 1 kΩ resistor)
2. Put thunder sounds (at least 15 s long) on the SD card in three folders:
   - `/01/001.mp3`, `/01/002.mp3` … close: sharp crack
   - `/02/001.mp3` … medium: rolling thunder
   - `/03/001.mp3` … distant: low rumble

   Set `THUNDER_TRACKS` in `thunder.h` to the number of files per folder.
3. Uncomment `#define ENABLE_THUNDER` in `cloud-lightning.ino` (Arduino) or `config.h` (ESP32).
4. Switch the sound on and off at runtime with `t` (Arduino) or the *Thunder sound* switch in Home Assistant.

### Real lightning from Blitzortung.org (optional)

Blitzortung.org only gives raw data to people who run their own detector station, and other apps have to use their own servers. So the cloud does not fetch the data itself. Instead it uses the [Blitzortung integration for Home Assistant](https://github.com/mrk-its/homeassistant-blitzortung), which is built for exactly this and already filters strikes by your coordinates and radius.

1. Install the Blitzortung integration (HACS) and set location and radius.
2. Copy `home-assistant/cloud_lightning.yaml` to `/config/packages/` (see the comments in the file).
3. WiFi: nothing else to do. Zigbee: switch to the `number.set_value` action described in the file.
4. Turn on *Lightning cloud shows real strikes* in Home Assistant.

Every strike inside your radius now flashes the cloud: close ones bright, distant ones as faint sheet lightning, with thunder at the real delay if sound is enabled. With several clouds, send the strikes to the leader. The Arduino + BLE version has no network connection and cannot receive real strikes.

## V1.1
[![Lightning cloud v1.1](http://i.imgur.com/i8TT3HJ.png)](https://www.youtube.com/watch?v=XI98PhaZPTs "Click to see the video")

Added control via Bluetooth Low Energy module. When you type the letter "f", the lightning "f"lashes.
[Instructable for adding BLE control available here.](http://www.instructables.com/id/How-to-Add-Bluetooth-Control-to-your-Lightning-Clo/step6/Test-with-the-Adafruit-BLE-app/)

To check out v1.1, use the following command:

    git checkout -b branch_name v1.1

## V1.0
[![Lightning cloud v1.0](http://i.imgur.com/79LUuV7.png)](https://www.youtube.com/watch?v=XxMMNcU-hWE "Click to see the video")

The basic lightning animation. Randomly generates flashes.  [Full Instructable available here.](http://www.instructables.com/id/How-to-make-a-Lightning-Cloud/)

To check out v1.0, use the following command:

    git checkout -b branch_name v1.0


## Early prototypes:
https://www.youtube.com/watch?v=283USS50E_s
