/*
 * Several clouds over Bluetooth LE: no WiFi, router or Home Assistant needed.
 *
 * The leader broadcasts every flash in its BLE advertising, the other clouds
 * listen (passive scan) and show it. The followers only need BLE, so they
 * can be small boards like an ESP32-C3 SuperMini.
 *
 * Needs the library "NimBLE-Arduino" (2.x).
 */

#include <NimBLEDevice.h>

// Advertising manufacturer data:
//   0-1   company id 0xFFFF (reserved for tests / non-commercial use)
//   2-3   "CL"
//   4     protocol version
//   5     SYNC_GROUP
//   6     flash counter (followers ignore repeats)
//   7-11  origin, spread, direction, glow, strokes
//   12-15 km (float)
//   16-19 seed
const uint8_t SYNC_VERSION = 1;
const size_t SYNC_PAYLOAD_SIZE = 20;

// The leader waits this long before showing a flash itself, so the followers
// (which receive it a few advertising intervals later) flash at the same time.
const unsigned long SYNC_LEADER_DELAY_MS = 40;

NimBLEAdvertising *syncAdvertising = nullptr;
uint8_t syncCounter = 0;
QueueHandle_t syncFlashes;

class SyncScanCallbacks : public NimBLEScanCallbacks {
  // Runs in the BLE task: only decode and queue, loop() shows the flash.
  void onResult(const NimBLEAdvertisedDevice *device) override {
    if (!device->haveManufacturerData()) {
      return;
    }
    std::string data = device->getManufacturerData();
    const uint8_t *d = reinterpret_cast<const uint8_t *>(data.data());
    if (data.size() != SYNC_PAYLOAD_SIZE || d[0] != 0xFF || d[1] != 0xFF || d[2] != 'C' ||
        d[3] != 'L' || d[4] != SYNC_VERSION || d[5] != SYNC_GROUP) {
      return;
    }
    // The leader repeats its last flash until the next one; show each only
    // once, and not the one that was already there when we started.
    bool first = !seenAny;
    seenAny = true;
    if (d[6] == lastCounter) {
      return;
    }
    lastCounter = d[6];
    if (first) {
      return;
    }

    Flash flash;
    flash.origin = d[7];
    flash.spread = d[8];
    flash.direction = (int8_t)d[9];
    flash.glow = d[10] != 0;
    flash.strokes = d[11];
    memcpy(&flash.km, d + 12, sizeof(flash.km));
    memcpy(&flash.seed, d + 16, sizeof(flash.seed));
    xQueueSend(syncFlashes, &flash, 0);
  }

  bool seenAny = false;
  uint8_t lastCounter = 0;
};

SyncScanCallbacks syncScanCallbacks;

void syncSetup() {
  NimBLEDevice::init("");
  if (CLOUD_LEADER) {
    syncAdvertising = NimBLEDevice::getAdvertising();
    syncAdvertising->setMinInterval(32);  // 20 ms (units of 0.625 ms)
    syncAdvertising->setMaxInterval(48);  // 30 ms
  } else {
    syncFlashes = xQueueCreate(4, sizeof(Flash));
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&syncScanCallbacks, true);  // also report repeats
    scan->setActiveScan(false);
    scan->setInterval(100);
    scan->setWindow(100);    // listen all the time
    scan->setMaxResults(0);  // don't keep results, only use the callback
    scan->start(0, false);   // scan forever
  }
}

void syncSendFlash(const Flash &flash) {
  uint8_t d[SYNC_PAYLOAD_SIZE] = {0xFF, 0xFF, 'C', 'L', SYNC_VERSION, SYNC_GROUP, ++syncCounter,
                                  flash.origin, flash.spread, (uint8_t)flash.direction,
                                  flash.glow, flash.strokes};
  memcpy(d + 12, &flash.km, sizeof(flash.km));
  memcpy(d + 16, &flash.seed, sizeof(flash.seed));

  NimBLEAdvertisementData advertisement;
  advertisement.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advertisement.setManufacturerData(std::string(reinterpret_cast<const char *>(d), sizeof(d)));
  syncAdvertising->stop();
  syncAdvertising->setAdvertisementData(advertisement);
  syncAdvertising->start();
  delay(SYNC_LEADER_DELAY_MS);
}

bool syncReceive(Flash &flash) {
  return !CLOUD_LEADER && xQueueReceive(syncFlashes, &flash, 0) == pdTRUE;
}
