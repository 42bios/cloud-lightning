/*
 * Music mode: an I2S microphone (INMP441 or similar, L/R pin to GND) makes
 * the cloud flash on the beat. Louder beats give brighter flashes that
 * light up more of the cloud, quiet ones only a faint glow.
 *
 * A beat is a short block that is clearly louder than the average of the
 * last second, so it adapts to the volume of the music by itself.
 */

#include <ESP_I2S.h>

const uint32_t MIC_SAMPLE_RATE = 16000;
const size_t MIC_BLOCK_SAMPLES = 256;      // 16 ms
const float MIC_AVERAGE_WEIGHT = 0.02;     // average over roughly one second
const float MIC_BEAT_RATIO = 1.6;          // how much louder than average a beat is
const unsigned long MIC_MIN_BEAT_GAP_MS = 180;

class BeatDetector {
public:
  bool begin() {
    if (started) {
      return true;
    }
    i2s.setPins(MIC_SCK_PIN, MIC_WS_PIN, -1, MIC_SD_PIN);
    i2s.setTimeout(5);
    started = i2s.begin(I2S_MODE_STD, MIC_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                        I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
    return started;
  }

  // Call from loop(). Returns the distance in km of a flash matching the
  // beat that was just heard (close = loud), or -1.
  float update() {
    if (!started) {
      return -1;
    }
    int32_t samples[MIC_BLOCK_SAMPLES];
    size_t bytes = i2s.readBytes(reinterpret_cast<char *>(samples), sizeof(samples));
    size_t count = bytes / sizeof(int32_t);
    if (count == 0) {
      return -1;
    }

    // Loudness of this block: mean absolute amplitude (24-bit data).
    float level = 0;
    for (size_t i = 0; i < count; i++) {
      level += abs(samples[i] >> 8);
    }
    level /= count;

    float ratio = average > 0 ? level / average : 0;
    average = average * (1 - MIC_AVERAGE_WEIGHT) + level * MIC_AVERAGE_WEIGHT;

    if (level < MIC_NOISE_FLOOR || ratio < MIC_BEAT_RATIO ||
        millis() - lastBeatAt < MIC_MIN_BEAT_GAP_MS) {
      return -1;
    }
    lastBeatAt = millis();
    // 1.6x average: faint (40 km), 4x and more: bright (1 km).
    float strength = constrain((ratio - MIC_BEAT_RATIO) / 2.4f, 0.0f, 1.0f);
    return 40 - strength * 39;
  }

private:
  I2SClass i2s;
  bool started = false;
  float average = 0;
  unsigned long lastBeatAt = 0;
};
