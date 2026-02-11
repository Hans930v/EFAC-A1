/*NOTE: Adjust ranges based on reliable readings.
Buffer of +600ms for Gcode side, for example, in my
tests, I had an average of 400ms per read, so I've put
M400 S1 in the === Filament slot communication ===
block*/

#include <Wire.h>
#include <VL53L0X.h>

VL53L0X sensor;

unsigned long lastCheck = 0;
const unsigned long checkInterval = 50;

int lastStable = -1;
int candidate = -1;
int stableCount = 0;
const int requiredStable = 3;

unsigned long candidateStartTime = 0; // track when candidate was first seen

struct SlotRange { int min; int max; }; 
SlotRange slots[] = { 
  {38,43}, {50,55}, {60,65}, {70,75}, //1-4 
  {79,84}, {90,95}, {102,107}, {116,121}, //5-8 
  {121,139}, {147,155}, {162,168}, {180,185}, //9-12 
  {193,198}, {212,217}, {222,231}, {238,249}, //13-16 
  {254,259}, {263,270}, {272,280}, {284,289}, //17-20 
  {292,298}, {299,305}, {309,317} //21-23 
  };

const int slotCount = sizeof(slots)/sizeof(slots[0]);

int slotFromDistance(int dist) {
  // dead zone between slot 21 and 22
  if (dist >= 298 && dist <= 299) return -1;

  for (int i = 0; i < slotCount; i++) {
    if (dist >= slots[i].min && dist <= slots[i].max) return i+1;
  }
  return -1;
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("Failed to detect and initialize sensor!");
    while (1) {}
  }
  sensor.setMeasurementTimingBudget(200000);
}

void loop() {
  unsigned long now = millis();
  if (now - lastCheck >= checkInterval) {
    lastCheck = now;

    int reading = sensor.readRangeSingleMillimeters();
    if (sensor.timeoutOccurred()) return;

    int slot = slotFromDistance(reading);

    if (slot == -1) {
      Serial.print("Distance: ");
      Serial.print(reading);
      Serial.println(" mm (Out of range)");
      candidate = -1;
      stableCount = 0;
      return;
    }

    if (candidate == -1) {
      candidate = slot;
      stableCount = 1;
      candidateStartTime = now; // mark when candidate started
      return;
    }

    if (slot == candidate) {
      stableCount++;
      if (stableCount >= requiredStable && slot != lastStable) {
        lastStable = slot;
        unsigned long elapsed = now - candidateStartTime; // calculate stabilization time
        Serial.print("Stabilized Distance: ");
        Serial.print(reading);
        Serial.print(" mm -> Slot ");
        Serial.print(lastStable);
        Serial.print(" (Elapsed: ");
        Serial.print(elapsed);
        Serial.println(" ms)");
      }
    } else {
      candidate = slot;
      stableCount = 1;
      candidateStartTime = now; // reset start time for new candidate
    }
  }
}
