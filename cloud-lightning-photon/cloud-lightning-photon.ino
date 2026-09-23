// This #include statement was automatically added by the Particle IDE.
#include "neopixel/neopixel.h"
#include "lightning.h"

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

    Particle.function("lightning", triggerWeather);
}

void loop() {
    // The animation runs here, so the cloud function returns immediately.
    lightning.update();
}

/**
 * "f" = start a short thunderstorm, returns 1
 * "a" = toggle ambient mode, returns 1 if now on, 0 if now off
 * "s" = stop everything, returns 0
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

    if (command == "s") {
        lightning.stop();
        return 0;
    }

    return -1;
}
