/*
 * Several clouds over Bluetooth LE: no WiFi, router or Home Assistant needed.
 *
 * The leader broadcasts every flash in its BLE advertising, the other clouds
 * (followers) listen and show it. Followers only need BLE, so they can be
 * small boards like an ESP32-C3 SuperMini.
 *
 * Pairing is automatic: switch on a new follower right next to the leader
 * (a few cm, antennas close together). It pulses blue while it searches,
 * joins the leader's installation and flashes twice. Pair the clouds in the
 * order they hang, starting next to the leader: the first becomes position
 * 1, the next 2 and so on. A follower remembers its leader; hold its button
 * for 6 s to forget it and pair again. Holding the leader's button for 6 s
 * starts a new installation (then pair all followers again).
 *
 * Needs the library "NimBLE-Arduino" (2.x).
 */

#include <NimBLEDevice.h>

// Manufacturer data of all frames:
//   0-1  company id 0xFFFF (reserved for tests / non-commercial use)
//   2-3  "CL", 4 protocol version, 5 frame kind
//
// Leader frame (kind 1), sent all the time:
//   6-9   installation id
//   10    number of clouds
//   11    flash counter (followers show each flash once)
//   12    origin, 13 spread
//   14    bits 0-1 direction+1, bit 2 glow
//   15    strokes
//   16-17 km * 100
//   18-21 seed
//   22-24 welcome: chip id of the follower that just joined (0 = none)
//   25    welcome: its position
//
// Join frame (kind 2), sent by a follower next to the leader:
//   6-9   installation id it wants to join
//   10-12 its chip id
const uint8_t SYNC_VERSION = 2;
const uint8_t SYNC_KIND_LEADER = 1;
const uint8_t SYNC_KIND_JOIN = 2;
const size_t SYNC_LEADER_SIZE = 26;
const size_t SYNC_JOIN_SIZE = 13;
const uint8_t SYNC_MAX_CLOUDS = 8;

// The leader waits this long before showing a flash itself, so the followers
// (which receive it a few advertising intervals later) flash at the same time.
const unsigned long SYNC_LEADER_DELAY_MS = 40;

struct SyncFrame {
  uint8_t data[SYNC_LEADER_SIZE];
  uint8_t size;
  int8_t rssi;
};

QueueHandle_t syncFrames;
Preferences syncSettings;
NimBLEAdvertising *syncAdvertising;

uint32_t syncInstallation = 0;  // leader: own id; follower: id of its leader
uint32_t syncChip = 0;          // 24-bit id of this board
bool syncPaired = false;        // follower only
uint8_t syncCloudCount = 1;     // leader only
uint32_t syncMembers[SYNC_MAX_CLOUDS];

// Leader: what is in the advertising right now.
Flash syncLastFlash = {};
uint8_t syncCounter = 0;
uint32_t syncWelcomeChip = 0;
uint8_t syncWelcomePosition = 0;

// Follower: pairing progress and duplicate filter.
uint32_t syncCandidate = 0;
uint8_t syncCandidateHits = 0;
bool syncJoining = false;
bool syncSeenAny = false;
uint8_t syncLastCounter = 0;

class SyncScanCallbacks : public NimBLEScanCallbacks {
  // Runs in the BLE task: only filter and queue, syncLoop() does the rest.
  void onResult(const NimBLEAdvertisedDevice *device) override {
    if (!device->haveManufacturerData()) {
      return;
    }
    std::string data = device->getManufacturerData();
    const uint8_t *d = reinterpret_cast<const uint8_t *>(data.data());
    if (data.size() < 6 || data.size() > SYNC_LEADER_SIZE || d[0] != 0xFF || d[1] != 0xFF ||
        d[2] != 'C' || d[3] != 'L' || d[4] != SYNC_VERSION) {
      return;
    }
    SyncFrame frame;
    memcpy(frame.data, d, data.size());
    frame.size = data.size();
    frame.rssi = device->getRSSI();
    xQueueSend(syncFrames, &frame, 0);
  }
};

SyncScanCallbacks syncScanCallbacks;

uint32_t syncRead32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

void syncWrite32(uint8_t *p, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    p[i] = value >> (8 * i);
  }
}

void syncAdvertise(const uint8_t *data, size_t size) {
  NimBLEAdvertisementData advertisement;
  advertisement.setFlags(BLE_HS_ADV_F_BREDR_UNSUP);
  advertisement.setManufacturerData(std::string(reinterpret_cast<const char *>(data), size));
  syncAdvertising->stop();
  syncAdvertising->setAdvertisementData(advertisement);
  syncAdvertising->start();
}

// Leader: puts the current state (last flash, welcome) into the advertising.
void syncAdvertiseLeader() {
  const Flash &f = syncLastFlash;
  uint8_t d[SYNC_LEADER_SIZE] = {0xFF, 0xFF, 'C', 'L', SYNC_VERSION, SYNC_KIND_LEADER};
  syncWrite32(d + 6, syncInstallation);
  d[10] = syncCloudCount;
  d[11] = syncCounter;
  d[12] = f.origin;
  d[13] = f.spread;
  d[14] = (f.direction + 1) | (f.glow ? 4 : 0);
  d[15] = f.strokes;
  uint16_t km = constrain(f.km * 100, 0.0f, 65535.0f);
  d[16] = km;
  d[17] = km >> 8;
  syncWrite32(d + 18, f.seed);
  d[22] = syncWelcomeChip;
  d[23] = syncWelcomeChip >> 8;
  d[24] = syncWelcomeChip >> 16;
  d[25] = syncWelcomePosition;
  syncAdvertise(d, sizeof(d));
}

void syncAdvertiseJoin() {
  uint8_t d[SYNC_JOIN_SIZE] = {0xFF, 0xFF, 'C', 'L', SYNC_VERSION, SYNC_KIND_JOIN};
  syncWrite32(d + 6, syncCandidate);
  d[10] = syncChip;
  d[11] = syncChip >> 8;
  d[12] = syncChip >> 16;
  syncAdvertise(d, sizeof(d));
}

void syncSaveLeader() {
  syncSettings.putUInt("install", syncInstallation);
  syncSettings.putUChar("count", syncCloudCount);
  syncSettings.putBytes("members", syncMembers, sizeof(syncMembers));
}

void syncApplyLayout() {
  lightning.setLayout(CLOUD_LEADER ? syncCloudCount : SYNC_MAX_CLOUDS,
                      CLOUD_LEADER ? 0 : syncSettings.getUChar("position", 1));
}

// Leader: a follower right next to us wants to join.
void syncHandleJoin(const uint8_t *d, int8_t rssi) {
  if (rssi < PAIRING_RSSI || syncRead32(d + 6) != syncInstallation) {
    return;
  }
  uint32_t chip = d[10] | (uint32_t)d[11] << 8 | (uint32_t)d[12] << 16;
  uint8_t position = 0;
  for (uint8_t i = 1; i < syncCloudCount; i++) {
    if (syncMembers[i] == chip) {
      position = i;  // joined before, e.g. the welcome got lost
    }
  }
  if (position == 0) {
    if (syncCloudCount >= SYNC_MAX_CLOUDS) {
      return;
    }
    position = syncCloudCount++;
    syncMembers[position] = chip;
    syncSaveLeader();
    syncApplyLayout();
  }
  if (syncWelcomeChip != chip) {
    syncWelcomeChip = chip;
    syncWelcomePosition = position;
    syncAdvertiseLeader();
  }
}

// Follower: a frame from a leader.
void syncHandleLeader(const uint8_t *d, int8_t rssi) {
  uint32_t installation = syncRead32(d + 6);

  if (!syncPaired) {
    uint32_t welcome = d[22] | (uint32_t)d[23] << 8 | (uint32_t)d[24] << 16;
    if (syncJoining && installation == syncCandidate && welcome == syncChip) {
      // Welcome: we are in.
      syncPaired = true;
      syncJoining = false;
      syncInstallation = installation;
      syncSettings.putUInt("install", installation);
      syncSettings.putUChar("position", d[25]);
      syncAdvertising->stop();
      syncApplyLayout();
      for (int i = 0; i < 2; i++) {
        lightning.fill(120, 120, 160);
        delay(120);
        lightning.off();
        delay(150);
      }
      return;
    }
    // Only pair with a leader that is right next to us, seen several times.
    if (rssi >= PAIRING_RSSI && installation != 0) {
      if (installation == syncCandidate) {
        syncCandidateHits++;
      } else {
        syncCandidate = installation;
        syncCandidateHits = 1;
      }
      if (syncCandidateHits >= 5 && !syncJoining) {
        syncJoining = true;
        syncAdvertiseJoin();
      }
    }
    return;
  }

  if (installation != syncInstallation) {
    return;
  }
  // The leader repeats its last flash until the next one: show each only
  // once, and not the one that was already there when we started.
  bool first = !syncSeenAny;
  syncSeenAny = true;
  if (d[11] == syncLastCounter) {
    return;
  }
  syncLastCounter = d[11];
  if (first) {
    return;
  }
  Flash flash;
  flash.origin = d[12];
  flash.spread = d[13];
  flash.direction = (int8_t)(d[14] & 3) - 1;
  flash.glow = d[14] & 4;
  flash.strokes = d[15];
  flash.km = (d[16] | d[17] << 8) / 100.0;
  flash.seed = syncRead32(d + 18);
  commandFlash(flash);
}

void syncSetup() {
  uint64_t mac = ESP.getEfuseMac();
  syncChip = (mac >> 24) & 0xFFFFFF;
  syncFrames = xQueueCreate(16, sizeof(SyncFrame));
  syncSettings.begin("sync", false);

  NimBLEDevice::init("");
  syncAdvertising = NimBLEDevice::getAdvertising();
  syncAdvertising->setMinInterval(32);  // 20 ms (units of 0.625 ms)
  syncAdvertising->setMaxInterval(48);  // 30 ms

  if (CLOUD_LEADER) {
    syncInstallation = syncSettings.getUInt("install", 0);
    if (syncInstallation == 0) {
      syncInstallation = esp_random() | 1;
    }
    syncCloudCount = constrain(syncSettings.getUChar("count", 1), 1, SYNC_MAX_CLOUDS);
    memset(syncMembers, 0, sizeof(syncMembers));
    syncSettings.getBytes("members", syncMembers, sizeof(syncMembers));
    syncSaveLeader();
    syncAdvertiseLeader();
  } else {
    syncInstallation = syncSettings.getUInt("install", 0);
    syncPaired = syncInstallation != 0;
  }
  syncApplyLayout();

  // Everyone listens: followers for the leader, the leader for join requests.
  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&syncScanCallbacks, true);  // also report repeats
  scan->setActiveScan(false);
  scan->setInterval(100);
  scan->setWindow(100);    // listen all the time
  scan->setMaxResults(0);  // don't keep results, only use the callback
  scan->start(0, false);   // scan forever
}

// Call from loop(): pairing and incoming flashes.
void syncLoop() {
  SyncFrame frame;
  while (xQueueReceive(syncFrames, &frame, 0) == pdTRUE) {
    const uint8_t *d = frame.data;
    if (CLOUD_LEADER && d[5] == SYNC_KIND_JOIN && frame.size == SYNC_JOIN_SIZE) {
      syncHandleJoin(d, frame.rssi);
    } else if (!CLOUD_LEADER && d[5] == SYNC_KIND_LEADER && frame.size == SYNC_LEADER_SIZE) {
      syncHandleLeader(d, frame.rssi);
    }
  }

  // Unpaired follower: pulse blue while searching, faster once it is joining.
  if (!CLOUD_LEADER && !syncPaired) {
    unsigned long period = syncJoining ? 400 : 2000;
    unsigned long phase = millis() % period;
    uint8_t v = 4 + 36 * (phase < period / 2 ? phase : period - phase) / (period / 2);
    lightning.fill(0, 0, v);
  }
}

// Leader: send a flash to the followers.
void syncSendFlash(const Flash &flash) {
  if (syncCloudCount < 2) {
    return;
  }
  syncLastFlash = flash;
  syncCounter++;
  syncAdvertiseLeader();
  delay(SYNC_LEADER_DELAY_MS);
}

// Hold the button for 6 s: the leader starts a new installation, a follower
// forgets its leader. Both then pair again.
void syncReset() {
  if (CLOUD_LEADER) {
    syncInstallation = esp_random() | 1;
    syncCloudCount = 1;
    memset(syncMembers, 0, sizeof(syncMembers));
    syncWelcomeChip = 0;
    syncSaveLeader();
    syncAdvertiseLeader();
  } else {
    syncSettings.clear();
    syncInstallation = 0;
    syncPaired = false;
    syncJoining = false;
    syncCandidate = 0;
    syncCandidateHits = 0;
    syncSeenAny = false;
    syncAdvertising->stop();
  }
  syncApplyLayout();
}

bool syncIsPaired() {
  return CLOUD_LEADER || syncPaired;
}
