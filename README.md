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
- **Optional thunder:** with a DFPlayer Mini, thunder follows the flash with the real delay of sound (about 3 s per km) and gets quieter with distance. Beyond 15 km it stays silent. Sound can be switched on and off at runtime.
- **Optional real lightning:** through Home Assistant, the cloud can show real strikes around your location, reported by [Blitzortung.org](https://www.blitzortung.org).

It also fixes some problems of the old version:

- The old "moving average" brightness was really a random walk that drifted, so after a while almost every flash was at full brightness.
- Pixels were not switched off between the flashes of a burst.
- The Photon cloud function blocked with `delay()`. It now only starts the animation, which runs in `loop()`.
- The Arduino loop waited 1 s between reads, so BLE commands could take up to a second to react. The loop is now non-blocking between flashes.

### Which version?

| Folder | Hardware | Control |
| --- | --- | --- |
| `cloud-lightning/` | Arduino + Bluefruit BLE module | BLE app |
| `cloud-lightning-photon/` | Particle Photon (WiFi) | Particle cloud, `wifi-blink.html`, Home Assistant via REST |
| `cloud-lightning-esp32/` | ESP32 (WiFi) | Home Assistant via MQTT, appears as a device automatically |

For new builds the **ESP32** is recommended: it is cheap, needs no extra module or base station, and Home Assistant is only needed for remote control and real lightning. Everything else runs on the cloud itself.

### Commands (Arduino and Photon)

| Command | Effect |
| --- | --- |
| `f` | Start a short thunderstorm (3–6 flashes) |
| `a` | Toggle ambient mode (endless distant storm) |
| `t` | Toggle thunder sound |
| `s` | Stop everything |
| `b<km>` | Show a real strike at the given distance, e.g. `b12.5` |

On the Arduino, send the command over BLE (for example with the Adafruit Bluefruit app). On the Photon, call the `lightning` function with the command as the argument, for example with `cloud-lightning-photon/wifi-blink.html`. The Photon function returns `1` for `f` and `b`, `1`/`0` for on/off with `a` and `t` (`-2` for `t` if sound is not compiled in), `0` for `s` and `-1` for an unknown command.

The ESP32 version is controlled over MQTT. With Home Assistant MQTT discovery it shows up as a device with the buttons *Thunderstorm* and *Stop* and the switches *Ambient storm* and *Thunder sound*. Ambient mode and sound are remembered across restarts. Real strikes are published to `cloud-lightning/strike` as the distance in km.

### Configuration

- `NUM_LEDS` / `PIXEL_COUNT`: number of LEDs. More LEDs spread across the cloud look much more spatial than 4.
- `LIGHTNING_R/G/B`, `STORM_MEAN_PAUSE_MS`, `AMBIENT_MEAN_PAUSE_MS`, `CLOSE_STRIKE_KM`, `MAX_STRIKE_KM` in `lightning.h`: colour, storm pace and how distance changes a flash.
- ESP32: copy `config.example.h` to `config.h` and fill in WiFi, MQTT broker and LED settings. `config.h` is ignored by git.
- `lightning.h` and `thunder.h` hold the shared code. The copies in all sketch folders must stay identical, because Arduino and Particle only compile files inside the sketch folder. The GitHub workflow checks this and compiles the Arduino and ESP32 versions.

### Thunder (optional)

Uses a DFPlayer Mini MP3 module. No extra library is needed.

1. Wire the DFPlayer Mini: power, a speaker, and
   - Arduino: DFPlayer TX → pin 10, DFPlayer RX → pin 11 (through a 1 kΩ resistor)
   - Photon: DFPlayer TX → RX, DFPlayer RX → TX (through a 1 kΩ resistor)
   - ESP32: DFPlayer TX → GPIO 16, DFPlayer RX → GPIO 17 (through a 1 kΩ resistor)
2. Put thunder sounds on the SD card as `/mp3/0001.mp3`, `/mp3/0002.mp3`, `/mp3/0003.mp3` (set `THUNDER_TRACKS` in `thunder.h` to the number of files).
3. Uncomment `#define ENABLE_THUNDER` in the sketch (Arduino, Photon) or in `config.h` (ESP32).
4. Switch the sound on and off at runtime with `t` or the *Thunder sound* switch in Home Assistant.

### Real lightning from Blitzortung.org (optional)

Blitzortung.org only gives raw data to people who run their own detector station, and other apps have to use their own servers. So the cloud does not fetch the data itself. Instead it uses the [Blitzortung integration for Home Assistant](https://github.com/mrk-its/homeassistant-blitzortung), which is built for exactly this and already filters strikes by your coordinates and radius.

1. Install the Blitzortung integration (HACS) and set location and radius.
2. Copy `home-assistant/cloud_lightning.yaml` to `/config/packages/` (see the comments in the file).
3. ESP32: nothing else to do. Photon: switch the action to the `rest_command` described in the file.
4. Turn on *Lightning cloud shows real strikes* in Home Assistant.

Every strike inside your radius now flashes the cloud: close ones bright, distant ones as faint sheet lightning, with thunder at the real delay if sound is enabled. The Arduino + BLE version has no network connection and cannot receive real strikes.

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
