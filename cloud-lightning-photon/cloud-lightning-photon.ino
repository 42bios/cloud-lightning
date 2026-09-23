// This #include statement was automatically added by the Particle IDE.
#include "neopixel/neopixel.h"
#include "lightning.h"

// Optional thunder sound via a DFPlayer Mini MP3 module on Serial1 (TX/RX pins),
// see thunder.h. Uncomment when the module is connected. It can then still be
// switched on and off at runtime with the "t" command.
// #define ENABLE_THUNDER

#ifdef ENABLE_THUNDER
#include "thunder.h"
Thunder thunder(Serial1);
#endif

// IMPORTANT: Set pixel COUNT, PIN and TYPE
// More LEDs spread across the cloud make the flashes look more spatial.
#define PIXEL_COUNT 4
#define PIXEL_PIN D0
#define PIXEL_TYPE WS2812B


// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
//               note: if not specified, D2 is selected for you.
// Parameter 3 = pixel type [ WS2812, WS2812B, WS2811, TM1803 ]
//               note: if not specified, WS2812B is selected for you.
//               note: RGB order is automatically applied to WS2811,
//                     WS2812/WS2812B/TM1803 is GRB order.
//
// 800 KHz bitstream 800 KHz bitstream (most NeoPixel products ...
//                         ... WS2812 (6-pin part)/WS2812B (4-pin part) )
//
// 400 KHz bitstream (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//                   (Radio Shack Tri-Color LED Strip - TM1803 driver
//                    NOTE: RS Tri-Color LED's are grouped in sets of 3)

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);
CloudLightning lightning(strip);

void setup() {

    strip.begin(); // Sends the start protocol for the LEDs.
    strip.show(); // Initialize all pixels to 'off'

#ifdef ENABLE_THUNDER
    Serial1.begin(9600);
#endif

    Particle.function("lightning", triggerWeather);
}

void loop() {
    // The animation runs here, so the cloud function returns immediately.
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

/**
 * "f"     = start a short thunderstorm, returns 1
 * "a"     = toggle ambient mode, returns 1 if now on, 0 if now off
 * "t"     = toggle thunder sound, returns 1 if now on, 0 if now off,
 *           -2 if sound is not enabled in the firmware
 * "s"     = stop everything, returns 0
 * "b<km>" = real strike at the given distance, e.g. "b12.5", returns 1
 * anything else returns -1
 */
int triggerWeather(String command) {

    if (command == "f") {
        lightning.startStorm();
        return 1;
    }

    if (command == "a") {
        lightning.setAmbient(!lightning.ambient());
        return lightning.ambient() ? 1 : 0;
    }

    if (command == "t") {
#ifdef ENABLE_THUNDER
        thunder.setEnabled(!thunder.isEnabled());
        return thunder.isEnabled() ? 1 : 0;
#else
        return -2;
#endif
    }

    if (command == "s") {
        lightning.stop();
#ifdef ENABLE_THUNDER
        thunder.cancel();
#endif
        return 0;
    }

    if (command.startsWith("b")) {
        String distance = command.substring(1);
        distance.trim();
        if (distance.length() > 0 && distance.charAt(0) >= '0' && distance.charAt(0) <= '9') {
            lightning.strikeAt(distance.toFloat());
            return 1;
        }
    }

    return -1;
}
