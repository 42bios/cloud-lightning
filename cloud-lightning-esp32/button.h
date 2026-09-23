/*
 * The cloud's only button (BOOT by default): click and holding for 2, 6 or
 * 10 s. While it is held, holdLevel() tells how many of these thresholds
 * are already passed, so the cloud can blink once per threshold and you
 * know when to let go.
 */

enum ButtonAction {
  BUTTON_NONE,
  BUTTON_CLICK,
  BUTTON_HOLD_2S,
  BUTTON_HOLD_6S,
  BUTTON_HOLD_10S,
};

const unsigned long BUTTON_DEBOUNCE_MS = 30;
const unsigned long BUTTON_HOLD_MS[3] = {2000, 6000, 10000};

class Button {
public:
  explicit Button(uint8_t buttonPin) : pin(buttonPin) {}

  void begin() {
    pinMode(pin, INPUT_PULLUP);
  }

  // Call from loop(). Returns an action when the button is released.
  ButtonAction update() {
    bool pressed = digitalRead(pin) == LOW;
    if (pressed && !down) {
      down = true;
      pressedAt = millis();
    } else if (!pressed && down) {
      down = false;
      unsigned long held = millis() - pressedAt;
      if (held >= BUTTON_HOLD_MS[2]) {
        return BUTTON_HOLD_10S;
      }
      if (held >= BUTTON_HOLD_MS[1]) {
        return BUTTON_HOLD_6S;
      }
      if (held >= BUTTON_HOLD_MS[0]) {
        return BUTTON_HOLD_2S;
      }
      if (held >= BUTTON_DEBOUNCE_MS) {
        return BUTTON_CLICK;
      }
    }
    return BUTTON_NONE;
  }

  // 0-3: how many hold thresholds are passed while the button is down.
  uint8_t holdLevel() const {
    if (!down) {
      return 0;
    }
    unsigned long held = millis() - pressedAt;
    uint8_t level = 0;
    while (level < 3 && held >= BUTTON_HOLD_MS[level]) {
      level++;
    }
    return level;
  }

private:
  uint8_t pin;
  bool down = false;
  unsigned long pressedAt = 0;
};
