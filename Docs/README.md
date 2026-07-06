# PESA (Position-Encoded Slot Assignment)
## Communication Protocol Specification v1.0

**Author:** Hans930v (Hansoy)  
**Date of Publication:** 2026-07-06  
**Project:** EFAC-A1 (External Feeder–Assisted Filament Change for Bambu Lab A1)  

---

### Legal Notice & License

This document is a **defensive publication** intended to establish prior art. 
It is dedicated to the **public domain** under the **CC0 1.0 Universal (CC0 1.0) Public Domain Dedication**. 

By publishing it openly, the protocol is established as prior art.  
It may be freely used, copied, and referenced without restriction.

*(See [LICENSE](LICENSE) in this folder for the full legal text.)*

---

### 1. Overview

PESA is a unidirectional, physical-layer communication protocol designed to transmit digital slot/filament identifiers from a closed-firmware 3D printer (Bambu Lab A1) to an external microcontroller (ESP32/Arduino). 

Unlike traditional methods that rely on GPIO pins, audio cues, or user button presses, PESA encodes data using the **absolute Cartesian position of the printer's toolhead** along the X-axis. An external Time-of-Flight (ToF) ranging sensor reads this position to decode the intended filament slot.

---

### 2. Hardware Topology

| Component | Role | Location |
| :--- | :--- | :--- |
| **Bambu Lab A1 Toolhead** | **Transmitter** | Moves along the X-axis (gantry). |
| **VL53L0X ToF Sensor** | **Receiver** | Fixed to the **stationary purge wiper assembly** on the far left side of the gantry. Faces horizontally toward the side of the moving toolhead. |
| **ESP32 / Arduino** | **Decoder** | Reads the VL53L0X sensor and controls the external feeder hardware. |

**Important:** Because the sensor is fixed to the purge wiper and faces the side of the toolhead, the measured distance changes linearly as the toolhead moves along the X-axis. The external microcontroller translates this raw distance reading back into the toolhead's absolute X-coordinate.

---

### 3. Transmitter Encoding (Printer Side)

This block is inserted into the **"Filament Change G-code"** section of Bambu Studio (or any slicer that supports custom filament-change macros).

**Encoding Formula:**
`X_Target = -19 + (Slot_Index * 10)`  
*(Units: millimeters)*

**Parameter Definitions:**
- `Slot_Index`: An integer representing the desired filament slot. 
  - Valid range: `0` to `19` (supports up to 20 distinct filaments).
  - (Future expansion up to 28 slots is supported by extending the valid range.)
- `X_Target`: The absolute X-coordinate (in mm) the toolhead moves to.

**Timing & Dwell:**
- The movement executes at rapid traverse speed: `F18000` (18,000 mm/min).
- Upon reaching the target, the printer executes `M400 S2` — a **2-second dwell** (pause) to allow the mechanical system to settle and the external sensor to take a stable reading.

**Invalid State Handling:**
- If `Slot_Index` falls outside the valid range, the printer executes `M400 U1` — a user-initiated pause, signaling an error.

**G-Code Reference Implementation (exactly as used in EFAC-A1):**
```gcode
; === Filament number communication ===
{if next_extruder >= 0 && next_extruder <= 19}
G1 X{-19 + (next_extruder * 10)} F18000 ; safe slot move
M400 S2        ; 2-second dwell for sensor reading
{else}
M400 U1        ; invalid slot - pause and alert user
{endif}
```
---

### 4. Receiver Decoding (External Microcontroller)

The external microcontroller (ESP32/Arduino) runs the following logic to decode the transmitted slot from the raw VL53L0X distance readings.

#### 4.1 Calibration Step (One-Time User Setup)

Because the VL53L0X reports raw distance (mm), the user must first establish a baseline reference.

1. Home the printer (toolhead moves to X=-48.2).
2. Record the distance reported by the VL53L0X at X=-48.2. Define this baseline as **`D_0`**.
3. This single-point calibration is sufficient because the sensor is fixed, and the distance-to-X relationship is strictly linear.

4. Receiver Decoding (External Microcontroller)
The external microcontroller (ESP32/Arduino) runs the following logic to decode the transmitted slot from the raw VL53L0X distance readings.

4.1 Calibration Step (One-Time User Setup)
The EFAC-A1 receiver does not calculate the slot arithmetically from a single baseline. Instead, it uses a pre-calibrated lookup table with tolerance bands. This approach is more robust because it compensates for real-world non-linearities, sensor mounting variations, thermal drift, and minor mechanical misalignments.

Calibration procedure:

1. Insert the EFAC-A1 G-code macro into Bambu Studio.

2. For each slot i (from 0 to 19), send the printer to the corresponding G-code position: X = -19 + (i * 10).

3. At each position, record the raw distance (mm) reported by the VL53L0X sensor.

4. Store these recorded distances in the microcontroller's Config.h file as an array, for example:
```cpp
const int SLOT_CENTERS[SLOT_COUNT] = { 
    52,   // Slot 1 (X = -19)  - example value
    45,   // Slot 2 (X = -9)   - example value
    38,   // Slot 3 (X = 1)    - example value
    // ... up to Slot 20
};
```
The user also defines a tolerance value (e.g., SLOT_TOLERANCE = 3 mm) to account for minor mechanical play and sensor noise.

4.2 Runtime Decoding Logic
During normal operation, the microcontroller receives a raw distance reading (D_Measured) from the VL53L0X. It then executes the following logic:

Step 1: Loop through all pre-calibrated slot centers.
Step 2: Check if the measured distance falls within the tolerance band of any center:

| D_Measured - SLOT_CENTERS[i] | ≤ SLOT_TOLERANCE

Step 3:

- If a match is found, decode the slot as i + 1 (returning a 1-based slot number).
- If no match is found, return -1 (invalid slot).

Reference Implementation (from EFAC-A1 firmware):
```cpp
static int slotFromDistance(int dist) {
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (dist >= (SLOT_CENTERS[i] - SLOT_TOLERANCE) &&
        dist <= (SLOT_CENTERS[i] + SLOT_TOLERANCE))
      return i + 1;   // 1-based slot number
  }
  return -1;          // No match
}
```

4.3 Sampling Algorithm (Pseudocode)
To ensure a reliable reading, the microcontroller follows this sequence:
1. Wait 100ms after detecting movement (to let the sensor stabilize).
2. Discard the initial 10 readings (mechanical vibrations).
3. Take 10 rapid distance readings (spaced 10ms apart).
4. Discard the highest and lowest readings; average the remaining 8 to filter noise.
5. Wait 1500ms (allows the toolhead to fully settle after reaching the target).
6. Take the final stable distance reading (D_Measured).
7. Apply the Decoding Logic: 
   Find i where | D_Measured - SLOT_CENTERS[i] | ≤ SLOT_TOLERANCE.
   If found, Decoded_Slot = i + 1. If not found, Decoded_Slot = -1 (invalid).
8. Validate the result (see Section 6).

4.4 Tolerance
The system uses a user-configurable SLOT_TOLERANCE value (typically 2–4 mm) to define the acceptance band around each calibrated center. This accounts for minor mechanical overshoot, sensor noise, and thermal expansion. Any reading that falls outside all bands is rejected as invalid.

5. Protocol Timing Diagram
The following sequence describes the exact chronological flow of a single PESA transaction:

Step	Time (ms)	Event	Description
1	0	G-Code Start	Printer begins movement to X_Target at F18000.
2	< 1000	Traverse	Toolhead moves to the target coordinate.
3	1000	Movement Complete	Toolhead reaches X_Target. Microcontroller detects stable position.
4	1000 - 1500	Mechanical Settling	Microcontroller ignores this window (vibrations and overshoot).
5	1500	Final Reading	Microcontroller takes D_Measured (the decisive value).
6	1500 - 2000	Decoding & Validation	Microcontroller applies the decoding logic and validates the slot.
7	2000	Dwell Ends	Printer executes M400 S2 timeout and resumes next G-code block.
Total dwell time: Exactly 2000ms (2 seconds), enforced by M400 S2. The microcontroller has a 500ms window (1500ms to 2000ms) to take its final reading and validate it.

6. Error Handling & Safety
Robust error handling is built into both the transmitter (printer) and receiver (microcontroller) to prevent misprints or hardware damage.

6.1 Transmitter (Printer) Errors
Condition	G-Code Response	User Action
Slot_Index < 0 or > 19	M400 U1 (User Pause)	The printer pauses and waits for the user to resolve the issue (e.g., manually select a valid slot).
Sensor not detected	N/A (Printer ignores external hardware)	The printer continues normally; the external feeder simply won't activate (fail-safe).
6.2 Receiver (Microcontroller) Errors
Condition	Microcontroller Response	Safety Mechanism
D_Measured falls outside all tolerance bands	Decoding fails; no signal sent to the feeder.	LED/Serial alert. Printer is in M400 S2 dwell, so it waits; the user can abort the print.
VL53L0X I2C timeout or read error	Retry up to 3 times. If still failing, enter safe state.	Feeder does not move. Prevents accidental filament loading into the wrong slot.
Decoded slot = 0 (default)	System treats as "No Selection".	Feeder remains idle. This prevents the feeder from defaulting to Slot 1 accidentally.
6.3 Emergency Stop / User Override
If the user presses the printer's physical stop button during the dwell, the printer immediately aborts the filament change.

The microcontroller detects the printer's halt (the toolhead stops moving) and resets its state machine, waiting for the next filament change.

7. Future Expansion
While the current implementation supports up to 20 slots (Slots 0–19), the protocol is designed to be easily scalable.

7.1 Extending Slot Count (Up to 29)
The G-code already contains a commented-out section for 29 slots:
```gcode
; {if next_extruder >= 0 && next_extruder <= 28}
; G1 X{-19 + (next_extruder * 10)} F18000
; M400 S2
; {endif}}
```
To expand:

- Remove the 19 upper bound and change it to 28.
- The X-coordinate range becomes: X = -19 to X = 261 (a total span of 280mm).
- This fits within the Bambu Lab A1's physical X-axis travel, so a slight adjustment to the offset (X-19) or step size (X10) may be required in future hardware revisions.

8. Licensing Reminder
This document is not governed by the same licenses as the rest of the EFAC-A1 project.

Component	License	Rationale
This PESA Protocol Specification CC0 1.0 Universal (Public Domain)	is dedicated to the public domain to serve as unrestricted prior art.

Firmware (ESP32/Arduino code)	AGPL 3.0	Strong copyleft to ensure modifications remain open source.

G-code (Bambu Studio macros)	MIT	Permissive, as this is a derivative work of existing open-source G-code by Andrzej Leszkiewicz and Steven Wu.

3D Models & Schematics	CERN-OH-S v2	Strongly reciprocal hardware license; modifications must be shared under the same terms.

Version History
Version |	Date | Author |	Changes |
| :--- | :--- | :--- | :--- |
1.0	| 2026-07-06 |	Hans930v (Hansoy) |	Initial public release. Defensive publication. |

End of Specification.
