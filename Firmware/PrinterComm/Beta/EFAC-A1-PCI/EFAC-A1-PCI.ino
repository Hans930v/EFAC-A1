/*  
  EFAC-A1-Printer Communication Interface
  ----------------------------------------
  This code is for the Arduino nano that has the 
  VL53L0X ToF sensor, and the ky-003 module near 
  the cutter finger of the A1
    
  How this works:
  1. It waits for cutter to be pressed, the cutter arms the system, signaling that a filament change, unload, or filament tangle may be occurring.
  2. After the cutter has been pressed, it waits for the distance to be below 35mm, only then it will start timeout timer and  switch to the next case
  (Timeout acts as a failsafe: if the toolhead remains at the wiper (<35 mm) for 3s, the state resets to WAIT_CUTTER.)
  3. After the distance goes above 35mm, it starts slot encoding mode to tell the feeder what slot the printer needs
  4. Once a stable distance reading matches a defined slot range, the feeder is commanded to unload the current filament.
  5. When the toolhead returns to the wiper (<35 mm), the slot number is sent and the feeder is commanded to load the new filament.
  6. State goes back to wait for cutter once again

  Notes:
  - All timings are millis()-based (no delays).
  - Invalid slot readings are ignored until stable.
  - Slot ranges must be recalibrated for accuracy. (to be automated soon)
  - Stable count (default = 3) can be tuned to balance speed vs. reliability.
*/

// Experimental
#include <Wire.h>
#include <VL53L0X.h>

// --- Pin definitions ---
#define CUTTER_PIN 14  //A0 pin & Hall effect sensor for cutter detection

VL53L0X ToF;

// --- State machine definitions ---
enum EFACState {
  WAIT_CUTTER,
  WAIT_BELOW_35,
  WAIT_ABOVE_35,
  SLOT_ENCODING,
  UNLOAD_COMMAND,
  WAIT_BELOW_35_AGAIN,
  LOAD_COMMAND
};

EFACState state = WAIT_CUTTER;

// --- Variables ---
// do not change
int lastStable = -1;           // stores last confirmed slot number
int candidate = -1;            // stores current candidate slot number
int stableCount = 0;           // counts consecutive stable readings
const int requiredStable = 7;  // slot stable count

unsigned long candidateStartTime = 0;
unsigned long timeoutStart = 0;

// to be tuned
const unsigned long unloadTimeout = 10000;  // in ms

// Slot ranges
struct SlotRange {
  int min;
  int max;
};
// to be recalibrated & to be automated in the future
SlotRange slots[] = {
  /*38*/ { 35, 41 },     // 1
  /*50*/ { 46, 53 },     // 2
  /*60*/ { 57, 63 },     // 3
  /*71*/ { 67, 74 },     // 4
  /*82*/ { 79, 85 },     // 5
  /*92*/ { 88, 95 },     // 6
  /*104*/ { 100, 107 },  // 7
  /*117*/ { 112, 120 },  // 8
  /*133*/ { 125, 137 },  // 9
  /*147*/ { 142, 152 },  // 10
  /*165*/ { 157, 169 },  // 11
  /*178*/ { 174, 184 },  // 12
  /*196*/ { 190, 199 },  // 13
  /*212*/ { 206, 215 },  // 14
  /*227*/ { 220, 230 },  // 15
  /*242*/ { 235, 245 },  // 16
  /*253*/ { 250, 257 },  // 17
  /*267*/ { 263, 270 },  // 18
  /*276*/ { 273, 279 },  // 19
  /*285*/ { 282, 300 }   // 20
};
const int slotCount = sizeof(slots) / sizeof(slots[0]);

// --- Helper functions ---
int slotFromDistance(int dist) {
  for (int i = 0; i < slotCount; i++) {
    if (dist >= slots[i].min && dist <= slots[i].max) return i + 1;
  }
  return -1;
}

void sendFeederCommand(String cmd) {
  Serial.print("Feeder Command: ");
  Serial.println(cmd);
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(CUTTER_PIN, INPUT);

  ToF.setTimeout(500);
  if (!ToF.init()) {
    Serial.println("Failed to detect and initialize sensor!");
    while (1) {}
  }
  ToF.setMeasurementTimingBudget(200000);  // High Accuracy
  Serial.println("EFAC-A1 PrinterComms started");
}

// --- Loop ---
void loop() {
  unsigned long now = millis();

  int reading = ToF.readRangeSingleMillimeters();
  int cutter = digitalRead(CUTTER_PIN);

  if (ToF.timeoutOccurred()) return;

  switch (state) {
    case WAIT_CUTTER:  // waits for cutter to be pressed
      if (cutter == LOW) {
        Serial.println("Cutter pressed");
        state = WAIT_BELOW_35;
        timeoutStart = now;
      }
      break;

    case WAIT_BELOW_35:  // wait for distance to be below 35mm
      if (reading <= 35 && cutter == HIGH) {
        Serial.println("Toolhead at wiper (below 35 mm)");
        state = WAIT_ABOVE_35;
        timeoutStart = now;
      }
      break;

    case WAIT_ABOVE_35:  // wait for distance to be above 35mm
      if (reading > 35 && cutter == HIGH) {
        Serial.println("Toolhead left wiper (above 35 mm)");
        state = SLOT_ENCODING;
        candidate = -1;
        stableCount = 0;
        timeoutStart = now;
      }
      break;

    case SLOT_ENCODING:  // encode slot based on distance
      {
        int slot = slotFromDistance(reading);
        if (slot == -1) break;

        if (candidate == -1) {
          candidate = slot;
          stableCount = 1;
          candidateStartTime = now;
        } else if (slot == candidate) {
          stableCount++;
          if (stableCount >= requiredStable && slot != lastStable) {
            lastStable = slot;
            unsigned long elapsed = now - candidateStartTime;
            Serial.print("Encoded Slot: ");
            Serial.println(lastStable);
            sendFeederCommand("UNLOAD");
            state = WAIT_BELOW_35_AGAIN;
            timeoutStart = now;
          }
        } else {
          candidate = slot;
          stableCount = 1;
          candidateStartTime = now;
        }
        break;
      }

    case WAIT_BELOW_35_AGAIN:  // wait for distance to be below 35 once again
      if (reading <= 35) {
        Serial.println("Toolhead back at wiper (below 35 mm)");
        state = LOAD_COMMAND;
        timeoutStart = now;
      }
      break;

    case LOAD_COMMAND:  // send commands to EFAC-A1-Feeder
      sendFeederCommand("LOAD_F" + String(lastStable));
      state = WAIT_CUTTER;
      timeoutStart = 0;
      break;
  }

  // Failsafe
  if (timeoutStart != 0 && now - timeoutStart > unloadTimeout) {
    Serial.println("Timeout -> Filament Unload only");
    sendFeederCommand("UNLOAD");
    state = WAIT_CUTTER;
    timeoutStart = 0;
  }
}
