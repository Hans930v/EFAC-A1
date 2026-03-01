// EFAC-A1-Feeder Firmware
// Controled Tests
// Unfinished

// EXPERIMENTAL
#include <Adafruit_MCP23X17.h>
#include <Wire.h>

/*
// --- Pin definitions (per MCP23017) ---
#define AIN2_1 0   // A0
#define AIN1_1 1   // A1
#define BIN1_2 2   // A2
#define BIN2_2 3   // A3
#define AIN2_3 4   // A4
#define AIN1_3 5   // A5
#define STBY_1 6   // A6
#define BIN1_4 7   // A7
#define BIN2_4 15  // B7
*/

#define AIN2_1 15  // B7
#define AIN1_1 7   // A7
#define BIN1_2 5   // A5
#define BIN2_2 4   // A4
#define AIN2_3 3   // A3
#define AIN1_3 2   // A2
#define STBY_1 6   // A6
#define BIN1_4 1   // A1
#define BIN2_4 0   // A0

// --- Sensors ---
#define f1 14  // B6
#define f2 13  // B5
#define f3 12  // B4
#define f4 11  // B3

const uint8_t motorPins[] = {
  AIN2_1, AIN1_1,
  BIN1_2, BIN2_2,
  AIN2_3, AIN1_3,
  BIN1_4, BIN2_4,
  STBY_1
};
const uint8_t sensorPins[] = { f1, f2, f3, f4 };

uint8_t motor_step = 0;


// --- Globals ---
Adafruit_MCP23X17 mcps[8];  // max of 8 expanders
bool slotPresent[32];       // up to 8 MCPs × 4 slots
bool lastReported[32];      // last state printed
int activeSlot = 1;         // 1 means slot 1 loaded
uint8_t activeCount = 0;    // how many MCPs detected
unsigned long sensorTimer = 0;

// --- Utility ---
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

// --- Setup ---
void setup() {
  Serial.begin(115200);
  Wire.begin();

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

  // Startup presence scan
  for (int mcpIndex = 0; mcpIndex < activeCount; mcpIndex++) {
    engageMcpGears(mcpIndex);
    scanPresenceMcp(mcpIndex);
    disengageMcpGears(mcpIndex);
  }

  reportPresence();
}

// --- Loop ---
void loop() {
  if (delayMillis(sensorTimer, 100)) {
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

// --- Command handling ---
void handleCommand(String cmd) {
  if (cmd.startsWith("LOAD_F")) {
    int targetSlot = cmd.substring(6).toInt();

    if (activeSlot > 0) {
      controlSlotOutput(activeSlot, false);  // unload current slot
    } else {
      Serial.println("No active filament to unload.");
    }

    controlSlotOutput(targetSlot, true);  // load requested slot
  } else if (cmd == "UNLOAD") {
    if (activeSlot > 0) {
      controlSlotOutput(activeSlot, false);
    } else {
      Serial.println("No filament detected to unload.");
    }
  }
}

// --- Presence scan ---
void scanPresenceMcp(int mcpIndex) {
  for (int slot = 0; slot < 4; slot++) {
    int globalSlot = mcpIndex * 4 + slot;
    int sensorVal = mcps[mcpIndex].digitalRead(sensorPins[slot]);
    bool present = (sensorVal == LOW);  // LOW = filament present

    slotPresent[globalSlot] = present;

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

// --- Gear engagement placeholders ---
void engageMcpGears(int mcpIndex) {
  Serial.print("Engaging gears for MCP #");
  Serial.println(mcpIndex + 1);
}
void disengageMcpGears(int mcpIndex) {
  Serial.print("Disengaging gears for MCP #");
  Serial.println(mcpIndex + 1);
}

// --- Slot mapping ---
int slotToMcp(int slot) {
  return (slot - 1) / 4;
}
int slotToMotorIndex(int slot) {
  return (slot - 1) % 4;
}

// --- High-level slot routines ---
void loadSlot(int slot) {
  int mcpIndex = slotToMcp(slot);
  int motorIndex = slotToMotorIndex(slot);
  engageGear(slot);
  push(mcpIndex, motorIndex);
  disengageGear(slot);
  slotPresent[slot - 1] = true;
  Serial.print("LOAD complete for slot ");
  Serial.println(slot);
}

void unloadSlot(int slot) {
  int mcpIndex = slotToMcp(slot);
  int motorIndex = slotToMotorIndex(slot);
  engageGear(slot);
  pull(mcpIndex, motorIndex);
  disengageGear(slot);
  slotPresent[slot - 1] = false;
  Serial.print("UNLOAD complete for slot ");
  Serial.println(slot);
}

int detectCurrentSlot() {
  for (int i = 0; i < activeCount * 4; i++) {
    if (slotPresent[i]) {
      return i + 1;  // slot numbers are 1-based
    }
  }
  return -1;  // no slot detected
}

void unloadCurrent() {
  int slot = detectCurrentSlot();
  if (slot > 0) unloadSlot(slot);
  else Serial.println("No filament detected to unload.");
}

// --- Gear engagement per slot ---
void engageGear(int slot) {
  Serial.print("Engaging gear for slot ");
  Serial.println(slot);
}
void disengageGear(int slot) {
  Serial.print("Disengaging gear for slot ");
  Serial.println(slot);
}

// --- Simplified slot-only control ---
enum SlotAction {
  LOAD_ACTION,
  UNLOAD_ACTION
};
void controlSlotOutput(int slot, bool load) {
  int mcpIndex   = slotToMcp(slot);
  int motorIndex = slotToMotorIndex(slot);

  bool slotEmpty = (mcps[mcpIndex].digitalRead(sensorPins[motorIndex]) == HIGH);

  SlotAction action = load ? LOAD_ACTION : UNLOAD_ACTION;

  switch (action) {
    case LOAD_ACTION:
      if (!slotEmpty) {
        push(mcpIndex, motorIndex);
        activeSlot = slot;
        Serial.print("LOADED_F");
        Serial.println(slot);
      } else {
        Serial.print("F");
        Serial.print(slot);
        Serial.println("_EMPTY1");
      }
      break;

    case UNLOAD_ACTION:
      if (!slotEmpty) {
        pull(mcpIndex, motorIndex);
        activeSlot = -1;
        Serial.print("UNLOADED_F");
        Serial.println(slot);
      } else {
        Serial.print("F");
        Serial.print(slot);
        Serial.println("_EMPTY2");
      }
      break;
  }
}

// --- Motor controls ---
void getMotorPins(int motorIndex, uint8_t &pin1, uint8_t &pin2) {
  switch (motorIndex) {
    case 0:
      pin1 = AIN1_1;
      pin2 = AIN2_1;
      break;
    case 1:
      pin1 = BIN1_2;
      pin2 = BIN2_2;
      break;
    case 2:
      pin1 = AIN1_3;
      pin2 = AIN2_3;
      break;
    case 3:
      pin1 = BIN1_4;
      pin2 = BIN2_4;
      break;
  }
}

void push(int mcpIndex, int motorIndex) {
  mcps[mcpIndex].digitalWrite(STBY_1, HIGH);  // enable motor driver
  uint8_t pin1, pin2;
  getMotorPins(motorIndex, pin1, pin2);

  Serial.print("PUSH slot ");
  Serial.println(mcpIndex * 4 + motorIndex + 1);

  // Forward direction: pin1 HIGH, pin2 LOW
  mcps[mcpIndex].digitalWrite(pin1, HIGH);
  mcps[mcpIndex].digitalWrite(pin2, LOW);

  delay(2000);  // run motor for 2s

  // Stop motor
  mcps[mcpIndex].digitalWrite(pin1, LOW);
  mcps[mcpIndex].digitalWrite(pin2, LOW);
  mcps[mcpIndex].digitalWrite(STBY_1, LOW);  // disable motor driver
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

  delay(2000);  // run motor for 2s

  // Stop motor
  mcps[mcpIndex].digitalWrite(pin1, LOW);
  mcps[mcpIndex].digitalWrite(pin2, LOW);
  mcps[mcpIndex].digitalWrite(STBY_1, LOW);  // disable motor driver
}
