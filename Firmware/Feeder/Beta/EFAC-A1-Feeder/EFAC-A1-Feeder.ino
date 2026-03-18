// EFAC-A1-Feeder Firmware - State Machine Version
// unfinished
#include <Adafruit_MCP23X17.h>
#include <Wire.h>

// --- Pin definitions ---
#ifdef ORIGINAL_PINS
#define AIN2_1 0
#define AIN1_1 1
#define BIN1_2 2
#define BIN2_2 3
#define AIN2_3 4
#define AIN1_3 5
#define STBY_1 6
#define BIN1_4 7
#define BIN2_4 15
#else
#define AIN2_1 15
#define AIN1_1 7
#define BIN1_2 5
#define BIN2_2 4
#define AIN2_3 3
#define AIN1_3 2
#define STBY_1 6
#define BIN1_4 1
#define BIN2_4 0
#endif

#define f1 14
#define f2 13
#define f3 12
#define f4 11
#define clctr A0

const uint8_t motorPins[] = { AIN2_1, AIN1_1, BIN1_2, BIN2_2, AIN2_3, AIN1_3, BIN1_4, BIN2_4, STBY_1 };
const uint8_t sensorPins[] = { f1, f2, f3, f4 };

Adafruit_MCP23X17 mcps[8];
bool slotPresent[32];
bool lastReported[32];
int activeSlot = 1; // assumes slot 1 at start
uint8_t activeCount = 0;

// --- State Machine ---
enum SystemPhase { IDLE, UNLOAD, PUSHBACK, LOAD, FULL_UNLOAD };
SystemPhase currentPhase = IDLE;
int targetSlot = -1;
unsigned long sensorStart = 0;
const unsigned long sensorInterval = 100;

int detectCurrentSlot() {
  for (int i = 0; i < activeCount * 4; i++) {
    if (slotPresent[i]) return i + 1;
  }
  return -1;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(clctr, INPUT);

  for (uint8_t addr = 0x20; addr <= 0x27; addr++) {
    if (activeCount < 8 && mcps[activeCount].begin_I2C(addr)) {
      setupPins(mcps[activeCount]);
      activeCount++;
    }
  }
  Serial.println("Feeder Ready.");

  activeSlot = detectCurrentSlot();
  if (activeSlot > 0) {
    Serial.print("Active slot at startup: ");
    Serial.println(activeSlot);
  } else {
    Serial.println("No active slot at startup.");
  }

}

void loop() {
  unsigned long now = millis();

  // Periodic sensor scan
  if (now - sensorStart >= sensorInterval) {
    for (uint8_t i = 0; i < activeCount; i++) scanPresenceMcp(i);
    sensorStart = now;
  }

  // Handle Commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("LOAD_F")) {
      targetSlot = cmd.substring(6).toInt();

      // Refresh activeSlot if unknown
      if (activeSlot < 1) activeSlot = detectCurrentSlot();

      // If requested slot already active, do nothing
      if (activeSlot == targetSlot) {
        Serial.print("Slot ");
        Serial.print(targetSlot);
        Serial.println(" already active. No action taken.");
        targetSlot = -1;
        currentPhase = IDLE;
      } else {
        // If there is an active slot different from target, unload it first
        if (activeSlot > 0) {
          Serial.print("LOAD_F");
          Serial.print(targetSlot);
          Serial.println(": unloading current slot first.");
          currentPhase = UNLOAD;
        } else {
          Serial.print("LOAD_F");
          Serial.print(targetSlot);
          Serial.println(": no active slot, starting load.");
          currentPhase = LOAD;
        }
      }
    } else if (cmd == "UNLOAD") {
      // Refresh activeSlot if unknown
      if (activeSlot < 1) activeSlot = detectCurrentSlot();

      if (activeSlot > 0) {
        targetSlot = -1; // No new slot to load after
        Serial.print("UNLOAD: will unload slot ");
        Serial.println(activeSlot);
        currentPhase = UNLOAD;
      } else {
        Serial.println("UNLOAD: no filament detected to unload.");
      }
    } else if (cmd == "FULL_UNLOAD") {
      if (activeSlot < 1) activeSlot = detectCurrentSlot();

      if (activeSlot > 0) {
        targetSlot = -1; // No new slot to load after
        Serial.print("FULL_UNLOAD: will unload slot ");
        Serial.println(activeSlot);
        currentPhase = FULL_UNLOAD;
      } else {
        Serial.println("FULL_UNLOAD: no filament detected to unload.");
      }
    }
  }

  runMotorStateMachine();
}

void runMotorStateMachine() {
  if (currentPhase == IDLE) return;

  // Identify which slot/motor we are currently talking to
  int operationalSlot = (currentPhase == LOAD) ? targetSlot : activeSlot;
  if (operationalSlot < 1) {
    currentPhase = IDLE;
    return;
  }

  uint8_t mcpIdx = slotToMcp(operationalSlot);
  uint8_t motIdx = slotToMotorIndex(operationalSlot);

  // Real-time sensor reads (Non-blocking)
  int sLocal = mcps[mcpIdx].digitalRead(sensorPins[motIdx]);
  int sClctr = digitalRead(clctr);

  switch (currentPhase) {
    case UNLOAD:
      // Pull until both sensors are HIGH (Filament is clear of the path)
      if (sLocal == HIGH && sClctr == HIGH) {
        stop(mcpIdx, motIdx);
        currentPhase = PUSHBACK;
        Serial.print("Unloaded_F");
        Serial.println(activeSlot);
      } else {
        pull(mcpIdx, motIdx);
      }
      break;

    case PUSHBACK:
      // Push back until local sensor is LOW (Filament parked) and clctr is HIGH (Path clear)
      if (sLocal == LOW && sClctr == HIGH) {
        stop(mcpIdx, motIdx);
        activeSlot = -1;
        if (targetSlot != -1) {
          currentPhase = LOAD;
        } else {
          currentPhase = IDLE;
          Serial.print("Unloaded_F");
          Serial.println(activeSlot);
        }
      } else {
        push(mcpIdx, motIdx);
        Serial.println("Starting Pushback");
      }
      break;

    case LOAD:
      // Push until both sensors are LOW (Filament fully loaded)
      if (sLocal == LOW && sClctr == LOW) {
        stop(mcpIdx, motIdx);
        activeSlot = targetSlot;
        targetSlot = -1;
        currentPhase = IDLE;
        Serial.print("LOADED_F");
        Serial.println(activeSlot);
      } else {
        push(mcpIdx, motIdx);
      }
      break;

    case FULL_UNLOAD:
      if (sLocal == HIGH && sClctr == HIGH) {
        stop(mcpIdx, motIdx);
        Serial.print("Unload_F");
        Serial.println(activeSlot);
      } else {
        pull(mcpIdx, motIdx);
      }
      break;
  }
}

// --- Helper Functions ---

void setupPins(Adafruit_MCP23X17 &expander) {
  for (uint8_t pin : motorPins) {
    expander.pinMode(pin, OUTPUT);
    expander.digitalWrite(pin, LOW);
  }
  for (uint8_t pin : sensorPins) expander.pinMode(pin, INPUT);
}

void push(uint8_t mcpIndex, uint8_t motorIndex) {
  uint8_t p1, p2; getMotorPins(motorIndex, p1, p2);
  mcps[mcpIndex].digitalWrite(STBY_1, HIGH);
  mcps[mcpIndex].digitalWrite(p1, HIGH);
  mcps[mcpIndex].digitalWrite(p2, LOW);
}

void pull(uint8_t mcpIndex, uint8_t motorIndex) {
  uint8_t p1, p2; getMotorPins(motorIndex, p1, p2);
  mcps[mcpIndex].digitalWrite(STBY_1, HIGH);
  mcps[mcpIndex].digitalWrite(p1, LOW);
  mcps[mcpIndex].digitalWrite(p2, HIGH);
}

void stop(uint8_t mcpIndex, uint8_t motorIndex) {
  uint8_t p1, p2; getMotorPins(motorIndex, p1, p2);
  mcps[mcpIndex].digitalWrite(p1, LOW);
  mcps[mcpIndex].digitalWrite(p2, LOW);
  mcps[mcpIndex].digitalWrite(STBY_1, LOW);
}

void getMotorPins(uint8_t motorIndex, uint8_t &pin1, uint8_t &pin2) {
  switch (motorIndex) {
    case 0: pin1 = AIN1_1; pin2 = AIN2_1; break;
    case 1: pin1 = BIN1_2; pin2 = BIN2_2; break;
    case 2: pin1 = AIN1_3; pin2 = AIN2_3; break;
    case 3: pin1 = BIN1_4; pin2 = BIN2_4; break;
  }
}

uint8_t slotToMcp(int slot) {
  return (slot - 1) / 4;
}
uint8_t slotToMotorIndex(int slot) {
  return (slot - 1) % 4;
}

void scanPresenceMcp(uint8_t mcpIndex) {
  for (int slot = 0; slot < 4; slot++) {
    uint8_t globalSlot = mcpIndex * 4 + slot;
    bool present = (mcps[mcpIndex].digitalRead(sensorPins[slot]) == LOW);
    slotPresent[globalSlot] = present;
    if (slotPresent[globalSlot] != lastReported[globalSlot]) {
      lastReported[globalSlot] = slotPresent[globalSlot];
      Serial.print("Slot "); Serial.print(globalSlot + 1);
      Serial.println(present ? ": PRESENT" : ": EMPTY");
    }
  }
}

void reportPresence() {
  for (uint8_t i = 0; i < activeCount * 4; i++) {
    Serial.print("Slot "); Serial.print(i + 1);
    Serial.println(slotPresent[i] ? ": PRESENT" : ": EMPTY");
  }
}
