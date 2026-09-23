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
- **Realistic timing:** pauses between flashes are exponentially distributed (mostly short, occasionally long).
- **Ambient mode:** an endless distant storm, mostly dim, diffuse sheet lightning with an occasional close strike.
- **Optional thunder** (Arduino version): with a DFPlayer Mini, close strikes rumble soon and loud, farther ones later and quieter, and sheet lightning stays silent.

It also fixes some problems of the old version:

- The old "moving average" brightness was really a random walk that drifted, so after a while almost every flash was at full brightness.
- Pixels were not switched off between the flashes of a burst.
- The Photon cloud function blocked with `delay()`. It now only starts the animation, which runs in `loop()`.
- The Arduino loop waited 1 s between reads, so BLE commands could take up to a second to react. The loop is now non-blocking between flashes.

### Commands

| Command | Effect |
| --- | --- |
| `f` | Start a short thunderstorm (3–6 flashes) |
| `a` | Toggle ambient mode (endless distant storm) |
| `s` | Stop everything |

On the Arduino, send the command over BLE (for example with the Adafruit Bluefruit app). On the Photon, call the `lightning` function with the command as the argument, for example with `cloud-lightning-photon/wifi-blink.html`. The Photon function returns `1` for `f`, `1`/`0` for ambient on/off with `a`, `0` for `s` and `-1` for an unknown command.

### Configuration

- `NUM_LEDS` / `PIXEL_COUNT`: number of LEDs. More LEDs spread across the cloud look much more spatial than 4.
- `LIGHTNING_R/G/B`, `STORM_MEAN_PAUSE_MS`, `AMBIENT_MEAN_PAUSE_MS` in `lightning.h`: colour and storm pace.
- `lightning.h` holds the shared animation code. The copies in `cloud-lightning/` and `cloud-lightning-photon/` must stay identical, because Arduino and Particle only compile files inside the sketch folder.

### Thunder (optional, Arduino)

1. Install the `DFRobotDFPlayerMini` library.
2. Wire the DFPlayer Mini: DFPlayer TX → pin 10, DFPlayer RX → pin 11 (through a 1 kΩ resistor), plus power and a speaker.
3. Put thunder sounds on the SD card as `0001.mp3`, `0002.mp3`, `0003.mp3` (set `THUNDER_TRACKS` to the number of files).
4. Uncomment `#define ENABLE_THUNDER` in `cloud-lightning.ino`.

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
