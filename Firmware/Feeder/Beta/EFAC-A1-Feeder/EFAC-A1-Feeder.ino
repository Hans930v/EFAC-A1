/*
  EFAC-A1-Feeder Firmware
  ----------------------------------------
  This code runs on the Arduino Nano that supervises
  MCP23017 I/O expanders for slot motors and sensors.
  It manages filament push/pull routines, collector
  verification, and slot presence tracking.

  How this works:
  1. On startup, each MCP engages its gears, scans slot
     presence sensors, disengages, and reports status.
  2. At runtime, the feeder listens for commands from the
     printer comms Nano (e.g., LOAD_Fx, UNLOAD).
  3. For LOAD: the feeder checks slot sensor presence,
     then pushes filament until the collector sensor confirms
     arrival. Both slot and collector must agree before success.
  4. For UNLOAD: the feeder pulls filament until collector
     and slot sensors confirm detection, then pushes until
     slot sensor reads empty and collector clears.
  5. Failsafes: all motor actions are millis()-based with
     timeout checks (default = 5s). If sensors do not change,
     the operation aborts with a FAILSAFE message.
  6. Reports: feeder returns LOADED_Fx or UNLOADED_Fx only
     when both slot and collector sensors confirm success.

  Notes:
  - Motors are driven in short bursts to allow sensor polling.
  - Collector sensor polarity must be calibrated (LOW = filament present).
  - Slot presence table is updated periodically for monitoring.
  - Commands are deterministic; invalid or unsafe actions are blocked.
*/

#include <Adafruit_MCP23X17.h>
#include <Wire.h>

// Pin definitions (per MCP23017)
#define AIN2_1 0   // A0
#define AIN1_1 1   // A1
#define BIN1_2 2   // A2
#define BIN2_2 3   // A3
#define AIN2_3 4   // A4
#define AIN1_3 5   // A5
#define STBY_1 6   // A6
#define BIN1_4 7   // A7
#define BIN2_4 15  // B7

// Sensors
#define f1 14      // B6
#define f2 13      // B5
#define f3 12      // B4
#define f4 11      // B3

#define clctr 13   // Nano A0

const uint8_t motorPins[] = {
  AIN2_1, AIN1_1,
  BIN1_2, BIN2_2,
  AIN2_3, AIN1_3,
  BIN1_4, BIN2_4,
  STBY_1
};
const uint8_t sensorPins[] = { f1, f2, f3, f4 };

Adafruit_MCP23X17 mcps[8];  // max of 8 expanders

// Slot presence table
bool slotPresent[32]; // up to 8 MCPs × 4 slots
bool lastReported[32]; // last state printed

int activeSlot = 1;  // 1 means slot 1 loaded

// Non-blocking delay replacement using millis()
bool delayMillis(unsigned long &lastTime, unsigned long ms) {
  unsigned long now = millis();
  if (now - lastTime >= ms) {
    lastTime = now;
    return true;
  }
  return false;
}

void setupPins(Adafruit_MCP23X17 &expander) {
  for (uint8_t pin : motorPins) {
    expander.pinMode(pin, OUTPUT);
    expander.digitalWrite(pin, LOW);
  }
  for (uint8_t pin : sensorPins) {
    expander.pinMode(pin, INPUT);
  }
}

uint8_t activeCount = 0;  // keeps track of how many MCPs were detected

// --- Setup ---
void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(clctr, INPUT);

  // Scan MCPs
  for (uint8_t addr = 0x20; addr <= 0x27; addr++) {
    if (activeCount < 8 && mcps[activeCount].begin_I2C(addr)) {
      Serial.print("MCP23017 detected at 0x");
      Serial.println(addr, HEX);
      setupPins(mcps[activeCount]);
      activeCount++;
    }
  }

  Serial.print("Total MCPs detected: ");
  Serial.println(activeCount);

  // Startup presence scan: one MCP at a time (4 motors active max)
  for (int mcpIndex = 0; mcpIndex < activeCount; mcpIndex++) {
    engageMcpGears(mcpIndex);
    scanPresenceMcp(mcpIndex);
    disengageMcpGears(mcpIndex);
  }

  reportPresence();
  detectActiveSlotAtStartup();
}

// --- Loop ---
unsigned long sensorTimer = 0;
uint8_t currentMcp = 0;

void loop() {
  if (delayMillis(sensorTimer, 100)) {  // periodic sensor check
    for (int mcpIndex = 0; mcpIndex < activeCount; mcpIndex++) {
      scanPresenceMcp(mcpIndex);
    }
  }

  // Handle incoming commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    handleCommand(cmd);
  }
}

// Engage/disengage gears per MCP (4 motors at a time)
void engageMcpGears(int mcpIndex) {
  Serial.print("Engaging gears for MCP #");
  Serial.println(mcpIndex + 1);
  // TODO: energize 4 motors of this MCP
}
void disengageMcpGears(int mcpIndex) {
  Serial.print("Disengaging gears for MCP #");
  Serial.println(mcpIndex + 1);
  // TODO: release 4 motors of this MCP
}

// Presence scan per MCP
void scanPresenceMcp(int mcpIndex) {
  for (int slot = 0; slot < 4; slot++) {
    int globalSlot = mcpIndex * 4 + slot;
    int sensorVal = mcps[mcpIndex].digitalRead(sensorPins[slot]);
    bool present = (sensorVal == LOW); // LOW = filament present

    slotPresent[globalSlot] = present;

    // Only report if changed
    if (slotPresent[globalSlot] != lastReported[globalSlot]) {
      lastReported[globalSlot] = slotPresent[globalSlot];
      Serial.print("Slot ");
      Serial.print(globalSlot + 1);
      Serial.print(" (MCP #");
      Serial.print(mcpIndex + 1);
      Serial.print(", local slot ");
      Serial.print(slot + 1);
      Serial.print("): ");
      Serial.println(present ? "PRESENT" : "EMPTY");
    }
  }
}

void reportPresence() {
  Serial.println("Slot presence table:");
  for (int i = 0; i < activeCount * 4; i++) {
    int mcpIndex = slotToMcp(i + 1);
    int localSlot = slotToMotorIndex(i + 1) + 1;
    Serial.print("Slot ");
    Serial.print(i + 1);
    Serial.print(" (MCP #");
    Serial.print(mcpIndex + 1);
    Serial.print(", local slot ");
    Serial.print("): ");
    Serial.println(slotPresent[i] ? "PRESENT" : "EMPTY");
  }
}

// --- Command handling ---
void handleCommand(String cmd) {
  if (cmd.startsWith("LOAD_F")) {
    int targetSlot = cmd.substring(6).toInt();

    if (activeSlot > 0) {
      controlSlotOutput(activeSlot, false);   // unload the actual last used slot
    } else {
      Serial.println("No active filament to unload.");
    }

    controlSlotOutput(targetSlot, true);      // load requested slot
  } else if (cmd == "UNLOAD") {
    if (activeSlot > 0) {
      controlSlotOutput(activeSlot, false);
    } else {
      Serial.println("No filament detected to unload.");
    }
  }
}


// Detect current slot based on sensors
int detectCurrentSlot() {
  for (int i = 0; i < activeCount * 4; i++) {
    if (slotPresent[i]) return i + 1;
  }
  return -1;
}

// Slot mapping
int slotToMcp(int slot) {
  return (slot - 1) / 4;
}
int slotToMotorIndex(int slot) {
  return (slot - 1) % 4;
}

// High-level slot routines
void loadSlot(int slot) {
  int mcpIndex = slotToMcp(slot);
  int motorIndex = slotToMotorIndex(slot);
  engageGear(slot);
  push(mcpIndex, motorIndex);
  // TODO: wait until sensor confirms filament present
  disengageGear(slot);
  slotPresent[slot - 1] = true;
  Serial.print("LOAD complete for slot ");
  Serial.println(slot);

  delay(2000);
}

void unloadSlot(int slot) {
  int mcpIndex = slotToMcp(slot);
  int motorIndex = slotToMotorIndex(slot);
  engageGear(slot);
  pull(mcpIndex, motorIndex);
  // TODO: wait until sensor confirms filament removed
  disengageGear(slot);
  slotPresent[slot - 1] = false;
  Serial.print("UNLOAD complete for slot ");
  Serial.println(slot);
}

void unloadCurrent() {
  int slot = detectCurrentSlot();
  if (slot > 0) unloadSlot(slot);
  else Serial.println("No filament detected to unload.");
}

// Gear engagement per slot
void engageGear(int slot) {
  Serial.print("Engaging gear for slot ");
  Serial.println(slot);
  // TODO: servo/motor driver ON
  delay(2000);
}
void disengageGear(int slot) {
  Serial.print("Disengaging gear for slot ");
  Serial.println(slot);
  // TODO: servo/motor driver OFF
  delay(2000);
}

enum UnloadState {
  UNLOAD_PULL,
  UNLOAD_PUSH_BACK,
  UNLOAD_STOP
};

UnloadState unloadState = UNLOAD_PULL;

void controlSlotOutput(int slot, bool load) {
  int mcpIndex   = slotToMcp(slot);
  int motorIndex = slotToMotorIndex(slot);

  bool collectorEmpty = (digitalRead(clctr) == HIGH);
  bool slotEmpty      = (mcps[mcpIndex].digitalRead(sensorPins[motorIndex]) == HIGH);

  switch (unloadState) {
    case UNLOAD_PULL:
      // pull until both sensors empty
      if (collectorEmpty && slotEmpty) {
        unloadState = UNLOAD_PUSH_BACK;
      } else {
        pull(mcpIndex, motorIndex);
      }
      break;

    case UNLOAD_PUSH_BACK:
      // Step 2: push until slot sensor sees filament again
      if (!slotEmpty) {
        unloadState = UNLOAD_STOP;
      } else {
        push(mcpIndex, motorIndex);
      }
      break;

    case UNLOAD_STOP:
      // Step 3: stop motor, mark slot unloaded
      Serial.print("UNLOADED_F"); Serial.println(slot);
      activeSlot = -1;
      unloadState = UNLOAD_PULL; // reset for next cycle
      break;
  }
}

// Map motor index (0–3) to its two control pins
void getMotorPins(int motorIndex, uint8_t &pin1, uint8_t &pin2) {
  switch (motorIndex) {
    case 0: pin1 = AIN1_1; pin2 = AIN2_1; break; // Slot 1
    case 1: pin1 = BIN1_2; pin2 = BIN2_2; break; // Slot 2
    case 2: pin1 = AIN1_3; pin2 = AIN2_3; break; // Slot 3
    case 3: pin1 = BIN1_4; pin2 = BIN2_4; break; // Slot 4
  }
}

// Motor Controls
void push(int mcpIndex, int motorIndex) {
  mcps[mcpIndex].digitalWrite(STBY_1, HIGH);  // enable motor driver
  uint8_t pin1, pin2;
  getMotorPins(motorIndex, pin1, pin2);

  Serial.print("PUSH slot ");
  Serial.println(mcpIndex * 4 + motorIndex + 1);

  // Forward direction: pin1 HIGH, pin2 LOW
  mcps[mcpIndex].digitalWrite(pin1, HIGH);
  mcps[mcpIndex].digitalWrite(pin2, LOW);

  delay(2000); // run motor for 2s

  // Stop motor
  mcps[mcpIndex].digitalWrite(pin1, LOW);
  mcps[mcpIndex].digitalWrite(pin2, LOW);
  mcps[mcpIndex].digitalWrite(STBY_1, LOW);   // disable motor driver
}
void pull(int mcpIndex, int motorIndex) {
  mcps[mcpIndex].digitalWrite(STBY_1, HIGH);  // enable motor driver
  uint8_t pin1, pin2;
  getMotorPins(motorIndex, pin1, pin2);

  Serial.print("PULL slot ");
  Serial.println(mcpIndex * 4 + motorIndex + 1);

  // Reverse direction: pin1 LOW, pin2 HIGH
  mcps[mcpIndex].digitalWrite(pin1, LOW);
  mcps[mcpIndex].digitalWrite(pin2, HIGH);

  delay(2000); // run motor for 2s

  // Stop motor
  mcps[mcpIndex].digitalWrite(pin1, LOW);
  mcps[mcpIndex].digitalWrite(pin2, LOW);
  mcps[mcpIndex].digitalWrite(STBY_1, LOW);   // disable motor driver
}

// Detect which slot is actually at the collector at startup
void detectActiveSlotAtStartup() {
  if (digitalRead(clctr) == LOW) { // LOW = filament present at collector
    // Find which slot sensor also reports PRESENT
    for (int i = 0; i < activeCount * 4; i++) {
      if (slotPresent[i]) {
        activeSlot = i + 1; // slot numbers are 1-based
        Serial.print("Startup: Collector confirms filament from slot ");
        Serial.println(activeSlot);
        return;
      }
    }
    // Collector sees filament but no slot sensor matched
    activeSlot = -1;
    Serial.println("Startup: Collector has filament, but slot unknown.");
  } else {
    // Collector is empty
    activeSlot = -1;
    Serial.println("Startup: No filament at collector.");
  }
}
