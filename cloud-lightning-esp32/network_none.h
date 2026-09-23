/*
 * Standalone: no WiFi, no Zigbee, no Home Assistant.
 *
 * The cloud runs its own random storm (ambient mode is on by default) and is
 * controlled with the BOOT button:
 *   short press       start a thunderstorm
 *   hold for 2 s      ambient mode on/off
 */

bool buttonWasPressed = false;
unsigned long buttonPressedAt = 0;
bool buttonHandled = false;

void networkSetup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void networkLoop() {
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  if (pressed && !buttonWasPressed) {
    buttonPressedAt = millis();
    buttonHandled = false;
  } else if (pressed && !buttonHandled && millis() - buttonPressedAt > 2000) {
    buttonHandled = true;
    commandAmbient(!lightning.ambient());
  } else if (!pressed && buttonWasPressed && !buttonHandled && millis() - buttonPressedAt > 30) {
    commandStorm();
  }
  buttonWasPressed = pressed;
}

void networkPublishState() {
}
