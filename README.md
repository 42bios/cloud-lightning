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
- **Continuing current:** some strokes keep glowing and flickering for up to 200 ms, as real ones do.
- **Afterglow:** every stroke fades out exponentially instead of switching off hard.
- **Colour:** the channel itself is white with a touch of blue. Light scattered through the cloud, coming from far away or fading out turns violet. So the LEDs around the channel, distant sheet lightning and the afterglow are more violet than the core of a close flash.
- **Gamma correction:** brightness is corrected for the non-linear perception of the eye, so fades look smooth.
- **Light spreads inside the cloud:** neighbouring LEDs glow along, and the channel sometimes wanders to the next LED.
- **Distance:** every flash has a distance. Close strikes are bright with several strokes, distant ones are faint, diffuse sheet lightning.
- **Realistic timing:** pauses between flashes are exponentially distributed (mostly short, occasionally long).
- **Ambient mode:** an endless distant storm, mostly sheet lightning with an occasional close strike.

All of the following is optional:

- **Thunder:** follows the flash with the real delay of sound (about 3 s per km). It cracks sharply when close and only rumbles when far, lasts longer for distant strikes and flashes with more strokes, gets quieter with distance and stays silent beyond 15 km.
- **Vibration:** a small vibration motor rumbles with close thunder: a hard jolt for a crack, then a decaying, uneven rumble. It is switched together with the thunder sound.
- **Several clouds:** an installation with e.g. 3 clouds shares one storm over Bluetooth LE, without WiFi or Home Assistant. New clouds pair automatically when switched on next to the main cloud. The clouds don't just show the same: a flash stays inside one cloud, jumps on from cloud to cloud, or lights the others faintly from the side, and each cloud flickers in its own way.
- **Music mode:** with a microphone the cloud flashes on the beat, without thunder.
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
| `cloud-lightning-esp32/` | ESP32, ESP32-C3, ESP32-C6 | button, Home Assistant over WiFi or Zigbee |

The ESP32 version is set up in `config.h`, choosing exactly one connectivity:

- **None:** standalone. The cloud runs its own random storm and is controlled with its button.
- **WiFi:** Home Assistant with an MQTT broker (e.g. the Mosquitto add-on). The cloud appears automatically via MQTT discovery.
- **Zigbee** (ESP32-C6 or -H2): your existing Zigbee coordinator (ZHA or Zigbee2MQTT). The cloud joins as a router, so it also strengthens your Zigbee mesh.

No base station is needed in any mode. Recommended boards: an **ESP32-C6** for the main cloud (it can do WiFi and Zigbee, so you can switch without new hardware) and a small **ESP32-C3** (e.g. SuperMini) for additional clouds.

### Button (ESP32)

Every ESP32 cloud has one button (the BOOT button, or your own on `BUTTON_PIN`). While you hold it, the cloud blinks once at 2, 6 and 10 s, so you know when to let go.

| | Main cloud (leader) | Additional cloud (follower) |
| --- | --- | --- |
| Click | Thunderstorm | Test flash |
| Double click | Music mode on/off | – |
| Hold 2 s | Ambient storm on/off | – |
| Hold 6 s | New installation: forget all paired clouds | Forget the main cloud, pair again |
| Hold 10 s | Zigbee: leave the network and pair again | – |

### Power and hanging

The clouds are meant to be powered permanently. WiFi, a Zigbee router and a BLE follower all listen all the time (a follower still draws about 80–100 mA), so a battery would last about a day. That is fine for an evening, not for permanent use.

The cord the cloud hangs from can carry the power:

- **Thin two-core cable as the cord:** e.g. transparent or textile-covered 2 × 0.5 mm² cable as used for pendant lamps. Use strain relief at both ends (a knot or cable grip inside the case) so the solder joints never carry the weight.
- **Low-voltage wire rope system** (tension wire system, as used for 12 V lighting): two tensioned steel wires carry 12 V along the ceiling, and each cloud hangs from them with two clamps. This works well for several clouds in a row.
- **12 V or 24 V instead of 5 V** on longer or thinner cables: a small step-down converter to 5 V in each case (e.g. a Mini560) keeps the current and voltage drop low.

Rough budget per cloud at 5 V: board 0.1–0.2 A, LEDs short peaks up to 60 mA each at full white, DFPlayer with speaker up to 0.5 A, vibration motor about 0.1 A. A 5 V / 2 A supply is plenty for one cloud with everything.

### Building the cloud for natural light

How the LEDs sit in the cloud matters as much as the code:

- **Never let the LEDs shine straight out.** Point them inwards or upwards onto a white inner shell or into the filling, so you see scattered light, not points.
- **Place LEDs at different depths:** some close under the surface (a close flash shows as a brighter spot), some deep inside (diffuse glow). The code already makes neighbouring LEDs glow along and more violet, which looks like light scattered through the cloud.
- **Order the LEDs along the cloud** (e.g. from left to right), because the flash spreads to neighbouring LED numbers and the channel wanders to the next one.
- **More LEDs spread out** (8–16 instead of 4) make the flashes far more spatial.
- **Filling:** pillow stuffing or cotton wool, loosely plucked and glued to the shell, diffuses best. Denser at the bottom, where you look from.
- **Vibration motor:** glue it to the shell, not to the case with the electronics, so the cloud itself trembles.

### Commands (Arduino + BLE)

| Command | Effect |
| --- | --- |
| `f` | Start a short thunderstorm (3–6 flashes) |
| `a` | Toggle ambient mode (endless distant storm) |
| `t` | Toggle thunder (sound and vibration) |
| `s` | Stop everything |
| `b<km>` | Show a real strike at the given distance, e.g. `b12.5` |

Send the commands over BLE, for example with the Adafruit Bluefruit app.

### ESP32 in Home Assistant

**WiFi:** the cloud shows up as a device with the buttons *Thunderstorm* and *Stop* and the switches *Ambient storm*, *Thunder* (sound and vibration) and *Music mode*. Real strikes are published to `<DEVICE_ID>/strike` as the distance in km.

**Zigbee:** the cloud offers on/off lights for *Thunderstorm* (turns off when the storm has ended), *Ambient storm*, *Thunder* and *Music mode*, and an analog output (a number in Home Assistant) that shows a real strike at the written distance in km. Rename the entities in Home Assistant as you like. It pairs automatically when it is not in a network yet. With ZHA the endpoints work directly. Zigbee2MQTT may need an external converter for the analog output.

In all modes, ambient mode and thunder are remembered across restarts.

### Several clouds (Bluetooth LE)

The clouds of an installation share one storm over Bluetooth LE. This works with every connectivity, also without WiFi and Home Assistant.

1. Main cloud: `#define ENABLE_SYNC` and `CLOUD_LEADER true`.
2. Additional clouds: `#define ENABLE_SYNC`, `CLOUD_LEADER false` and usually `CONNECTIVITY_NONE`.
3. Switch on a new cloud right next to the main cloud, a few cm apart. It pulses blue while searching, faster while joining, and flashes twice when it is paired. It remembers the main cloud from then on.
4. Pair the clouds in the order they hang, starting next to the main cloud: the first becomes position 1, the next 2 and so on. The flash jumps between neighbouring positions.

If pairing does not start inside the case, lower `PAIRING_RSSI` (e.g. to -60). To start over, hold the main cloud's button for 6 s (new installation) and each additional cloud's button for 6 s (forget the main cloud), then pair again.

The clouds don't simply show the same flash. Each flash starts in one cloud and is one of three kinds:

- **Intra-cloud flash:** it stays inside that cloud.
- **Jump:** the channel moves on to the neighbouring clouds in one direction, a moment later and weaker in each.
- **Big strike:** it lights the other clouds faintly from the side, where not every stroke is visible.

Distant sheet lightning lights all clouds faintly. The rhythm of the strokes is the same everywhere because it is one flash. Where it flickers, how bright and which colour is different in every cloud.

With thunder, either one cloud has a speaker and plays every thunder, or several clouds have speakers and each plays only the thunder of flashes that start in it (`THUNDER_OWN_FLASHES_ONLY true`). Then the thunder comes from the right direction.

BLE together with WiFi works on all ESP32 boards. BLE together with Zigbee on the ESP32-C6 compiles, but has not been tested on hardware yet.

### Music mode (optional)

An I2S microphone (INMP441 or similar, L/R pin to GND) on the main cloud makes all clouds flash on the beat. A beat is a short moment clearly louder than the last second, so it adapts to the volume by itself. Louder beats give brighter flashes that can jump between the clouds, quieter ones only a faint glow.

There is no thunder or vibration in music mode: the speaker would trigger the microphone, and thunder does not go with music. The storm pauses and comes back when music mode is switched off.

1. Wire the microphone (3.3 V, GND, SCK, WS, SD; pins in `config.h`).
2. Uncomment `#define ENABLE_MICROPHONE` in `config.h`.
3. Switch music mode on with a double click or in Home Assistant. If the cloud flashes in a quiet room, raise `MIC_NOISE_FLOOR`; if it misses quiet music, lower it.

### Configuration

- Arduino: `NUM_LEDS`, `LED_PIN`, `ENABLE_THUNDER` and `ENABLE_RUMBLE` in `cloud-lightning.ino`.
- ESP32: copy `config.example.h` to `config.h` and fill in connectivity, WiFi/MQTT, clouds, LEDs, thunder and microphone. `config.h` is ignored by git. Libraries: `Adafruit NeoPixel`; with WiFi also `PubSubClient`; with several clouds also `NimBLE-Arduino` (2.x).
- `lightning.h`: colours (`CORE_R/G/B`, `SCATTER_R/G/B`), storm pace (`STORM_MEAN_PAUSE_MS`, `AMBIENT_MEAN_PAUSE_MS`) and how distance changes a flash (`CLOSE_STRIKE_KM`, `MAX_STRIKE_KM`).
- `thunder.h`: number of sound files per folder (`THUNDER_TRACKS`), thunder length (`THUNDER_MIN_MS`, `THUNDER_MAX_MS`) and vibration (`RUMBLE_MAX_KM`, `RUMBLE_MIN_PWM`).
- `lightning.h` and `thunder.h` are shared. The copies in both sketch folders must stay identical, because Arduino only compiles files inside the sketch folder. The GitHub workflow checks this and compiles all variants.

### Thunder (optional)

**Sound** uses a DFPlayer Mini MP3 module. No extra library is needed.

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

**Vibration** uses a small vibration motor (coin or ERM, 3–5 V) switched by a logic-level MOSFET (e.g. AO3400) or an NPN transistor, with a flyback diode (e.g. 1N4148) across the motor. Never drive it straight from the pin.

1. Wire it to `RUMBLE_PIN`: Arduino pin 5, ESP32 GPIO 25, ESP32-C3 / -C6 GPIO 5.
2. Uncomment `#define ENABLE_RUMBLE`.

Switch sound and vibration on and off together with `t` (Arduino), in Home Assistant (*Thunder*), or they stay as saved.

### Real lightning from Blitzortung.org (optional)

Blitzortung.org only gives raw data to people who run their own detector station, and other apps have to use their own servers. So the cloud does not fetch the data itself. Instead it uses the [Blitzortung integration for Home Assistant](https://github.com/mrk-its/homeassistant-blitzortung), which is built for exactly this and already filters strikes by your coordinates and radius.

1. Install the Blitzortung integration (HACS) and set location and radius.
2. Copy `home-assistant/cloud_lightning.yaml` to `/config/packages/` (see the comments in the file).
3. WiFi: nothing else to do. Zigbee: switch to the `number.set_value` action described in the file.
4. Turn on *Lightning cloud shows real strikes* in Home Assistant.

Every strike inside your radius now flashes the cloud: close ones bright, distant ones as faint sheet lightning, with thunder at the real delay if it is enabled. With several clouds, send the strikes to the main cloud. The Arduino + BLE version has no network connection and cannot receive real strikes.

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
