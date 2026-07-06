# PESA (Position-Encoded Slot Assignment)
## Communication Protocol Specification v1.1

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

Unlike traditional methods that rely on GPIO pins, audio cues, or user button presses, PESA uses a **two-sensor architecture**:

- A **hall-effect sensor** mounted on the cutter side detects when the toolhead presses the filament cutter — this is the **arming signal** that tells PESA a filament event is occurring. Detection is **non-contact**: the sensor reads the magnetic field of a small magnet mounted on the toolhead, requiring no physical interaction between sensor and toolhead.
- A **VL53L0X Time-of-Flight sensor** mounted on the purge wiper side reads the toolhead's absolute X-axis position — this is the **data channel** that encodes which filament slot to load next.

The hall sensor arms the system. The ToF sensor reads the message. Together they form a complete, firmware-safe communication channel that requires no proprietary protocol access, no hardware modification to the printer, and no reverse engineering of Bambu Lab's firmware.

---

### 2. Hardware Topology

| Component | Role | Location |
| :--- | :--- | :--- |
| **Bambu Lab A1 Toolhead** | Transmitter | Moves along the X-axis (gantry). Carries a small magnet mount on the side of the toolhead. |
| **KY-003 Hall Effect Sensor** | Arming Signal | Fixed to the cutter side of the gantry. Detects the magnetic field of a small magnet mounted on the toolhead as it passes during a cutter press — **non-contact detection**, no physical interaction between sensor and toolhead required. |
| **VL53L0X ToF Sensor** | Data Receiver | Fixed to the stationary purge wiper assembly on the far left side of the gantry. Faces horizontally toward the side of the moving toolhead. Reads X-axis position as distance. |
| **ESP32 / Arduino** | Decoder | Reads both sensors and controls the external feeder hardware based on decoded slot. |

**Sensor roles are distinct and sequential:**
- The **hall sensor** fires first — it detects the cutter press and arms the PESA state machine. Without this signal, the ToF sensor readings are ignored.
- The **ToF sensor** is active throughout — but its readings are only acted upon after the hall sensor has armed the system and the toolhead has moved to the wiper and then out to the slot-encoded position.

**Important:** The hall sensor arms PESA on *any* cutter event — including filament tangles, errors, or manual cuts — not only intentional filament swaps. The state machine therefore does not act on the hall sensor alone. It waits for the full G-code sequence (wiper arrival → slot move → wiper return) before issuing a load command, ensuring false triggers do not cause unintended feeder activation.

**Important:** Because the ToF sensor is fixed to the purge wiper and faces the side of the toolhead, the measured distance changes linearly as the toolhead moves along the X-axis. The external microcontroller translates this raw distance reading back into the toolhead's absolute X-coordinate.

---

### 3. Transmitter Encoding (Printer Side)

This block is inserted into the **Filament Change G-code** section of Bambu Studio (or any slicer that supports custom filament-change macros).

**Encoding Formula:**

```
X_Target = -19 + (Slot_Index × 10)    (units: mm)
```

**Parameter Definitions:**

| Parameter | Description |
| :--- | :--- |
| `Slot_Index` | Integer representing the desired filament slot. Valid range: `0` to `19` (up to 20 distinct filaments). Future expansion up to `28` is supported. |
| `X_Target` | Absolute X-coordinate (mm) the toolhead moves to. |

**Timing & Dwell:**
- Movement executes at rapid traverse speed: `F18000` (18,000 mm/min).
- Upon reaching the target, the printer executes `M400 S2` — a **2-second dwell** to allow mechanical settling and stable sensor reading.

**Invalid State Handling:**
- If `Slot_Index` falls outside the valid range, the printer executes `M400 U1` — a user-initiated pause, signaling an error.

**G-Code Reference Implementation:**

```gcode
; === Filament number communication (PESA) ===
{if next_extruder >= 0 && next_extruder <= 19}
G1 X{-19 + (next_extruder * 10)} F18000 ; move toolhead to slot-encoded position
M400 S2                                  ; 2-second dwell for sensor reading
{else}
M400 U1                                  ; invalid slot — pause and alert user
{endif}
```

---

### 4. Receiver Decoding (External Microcontroller)

The external microcontroller implements a six-state event-driven state machine. The hall-effect sensor provides the **arming signal** that starts the transaction. The VL53L0X ToF sensor provides the **slot data**. Neither sensor alone is sufficient — both must fire in the correct sequence for a valid slot decode to occur.

#### 4.1 Calibration (One-Time User Setup)

The EFAC-A1 receiver uses a pre-calibrated lookup table with tolerance bands rather than arithmetic calculation from a single baseline. This approach is more robust because it compensates for real-world non-linearities, sensor mounting variations, thermal drift, and minor mechanical misalignments.

**Calibration Procedure:**

1. Insert the EFAC-A1 G-code macro into Bambu Studio.
2. For each slot `i` (from `0` to `19`), send the printer to: `X = -19 + (i × 10)`.
3. At each position, record the raw distance (mm) reported by the VL53L0X sensor.
4. Store the recorded distances in `Config.h` as a lookup array:

```cpp
constexpr int SLOT_CENTERS[] = {
    52,   // Slot 1  (X = -19) — example value
    45,   // Slot 2  (X = -9)  — example value
    38,   // Slot 3  (X =  1)  — example value
    // ... up to Slot 20
};
```

5. Define a tolerance value (e.g., `SLOT_TOLERANCE = 3`) to account for mechanical play and sensor noise.

---

#### 4.2 Runtime Decoding Logic

During normal operation, the microcontroller receives `D_Measured` from the VL53L0X and executes the following:

1. Loop through all pre-calibrated slot centers.
2. Check if the measured distance falls within the tolerance band of any center:

```
| D_Measured - SLOT_CENTERS[i] | ≤ SLOT_TOLERANCE
```

3. If a match is found → return `i + 1` (1-based slot number).  
   If no match is found → return `-1` (invalid slot).

**Reference Implementation:**

```cpp
static int slotFromDistance(int dist) {
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (dist >= (SLOT_CENTERS[i] - SLOT_TOLERANCE) &&
        dist <= (SLOT_CENTERS[i] + SLOT_TOLERANCE))
      return i + 1;   // 1-based slot number
  }
  return -1;          // no match — invalid slot
}
```

---

#### 4.3 State Machine Sequence

The receiver operates as a six-state non-blocking state machine, driven entirely by `millis()`-based timing with no blocking delays. The full transaction sequence for a valid PESA decode is:

| State | Trigger to Advance | Description |
| :--- | :--- | :--- |
| `WAIT_CUTTER` | Hall sensor goes LOW (magnet detected, non-contact) | System armed. Cutter has been pressed. Timeout timer starts. |
| `WAIT_BELOW_35` | ToF reading ≤ `WIPER_THRESHOLD` AND cutter released (HIGH) | Toolhead has arrived at the purge wiper. Cutter must be released first — confirms the arm has retracted before wiper detection begins. |
| `WAIT_ABOVE_35` | ToF reading > `WIPER_THRESHOLD` AND cutter released (HIGH) | Toolhead has left the wiper zone and is moving to the slot-encoded X position. |
| `SLOT_ENCODING` | `REQUIRED_STABLE` consecutive matching readings at the same slot center | ToF readings are accumulated. A candidate slot is confirmed only after multiple stable consecutive readings match the same `SLOT_CENTERS[]` entry within `SLOT_TOLERANCE`. Unstable or out-of-range readings reset the candidate window. |
| `WAIT_BELOW_35_AGAIN` | ToF reading ≤ `WIPER_THRESHOLD` | Toolhead has returned to the wiper. Confirms filament-swap position and prevents nozzle ooze on the print. |
| `LOAD_COMMAND` | Immediate | Load command enqueued with confirmed slot number. State machine resets to `WAIT_CUTTER`. |

**Slot confirmation requires stability, not averaging.** The firmware does not average multiple readings. Instead, `REQUIRED_STABLE` (default: 7) consecutive readings must all match the same slot center within tolerance before the slot is confirmed. A single out-of-range reading resets the candidate counter, making the decode robust against transient noise without introducing averaging latency.

**Global timeout:** A single timeout (`UNLOAD_TIMEOUT`, default: 10,000 ms) applies to every state after `WAIT_CUTTER`. If any state does not advance within this window, the feeder queue is flushed and `CMD_FULL_UNLOAD` is issued as a failsafe.

---

#### 4.4 Tolerance

The system uses a user-configurable `SLOT_TOLERANCE` value (typically 2–4 mm) to define the acceptance band around each calibrated center. This accounts for minor mechanical overshoot, sensor noise, and thermal expansion. Any reading outside all bands is rejected as invalid.

---

### 5. Protocol Timing Diagram

The following sequence describes the exact chronological flow of a complete PESA transaction, from filament cut to load command. Timing values are approximate — the state machine is event-driven, not clock-driven.

| Step | State | Trigger | Description |
| :---: | :--- | :--- | :--- |
| 1 | `WAIT_CUTTER` | Hall sensor LOW (non-contact) | Toolhead presses filament cutter. Magnet field detected. System armed. Timeout starts. |
| 2 | `WAIT_BELOW_35` | ToF ≤ 35 mm AND cutter HIGH | Cutter released. Toolhead moves to purge wiper. ToF detects arrival. |
| 3 | `WAIT_ABOVE_35` | ToF > 35 mm AND cutter HIGH | Toolhead leaves wiper zone, moves toward slot-encoded X position. |
| 4 | `SLOT_ENCODING` | `REQUIRED_STABLE` stable readings | ToF reads toolhead distance. Slot confirmed after 7 consecutive matching readings within `SLOT_TOLERANCE`. |
| 5 | `WAIT_BELOW_35_AGAIN` | ToF ≤ 35 mm | Toolhead returns to purge wiper. Confirms swap position, prevents ooze on print. |
| 6 | `LOAD_COMMAND` | Immediate | Load command enqueued with confirmed slot. State machine resets to `WAIT_CUTTER`. |

**G-code dwell window (Steps 3–4):**
- Toolhead moves to `X_Target` at `F18000`.
- Printer executes `M400 S2` — a **2-second dwell**.
- The microcontroller accumulates stable ToF readings during this window.
- The dwell must be long enough for `REQUIRED_STABLE` readings to accumulate before the toolhead moves away.

> **Total dwell time:** Exactly 2000 ms, enforced by `M400 S2`.  
> The microcontroller has the full 2-second window to confirm the slot before the printer resumes the G-code sequence.

---

### 6. Error Handling & Safety

Robust error handling is built into both the transmitter and receiver to prevent misprints or hardware damage.

#### 6.1 Transmitter (Printer) Errors

| Condition | G-Code Response | User Action |
| :--- | :--- | :--- |
| `Slot_Index` < 0 or > 19 | `M400 U1` (User Pause) | Printer pauses; user resolves the invalid slot selection. |
| Sensor not detected | N/A — printer ignores external hardware | Printer continues normally; external feeder does not activate (fail-safe). |

#### 6.2 Receiver (Microcontroller) Errors

| Condition | Microcontroller Response | Safety Mechanism |
| :--- | :--- | :--- |
| `D_Measured` outside all tolerance bands | Decoding fails; no signal sent to feeder. | LED/Serial alert. Printer is still in `M400 S2` dwell — user can abort. |
| VL53L0X I2C timeout or read error | Retry up to 3 times; enter safe state if still failing. | Feeder does not move. Prevents accidental loading into wrong slot. |
| Decoded slot = 0 (default) | Treated as "No Selection". | Feeder remains idle; prevents accidental default to Slot 1. |
| Hall sensor triggered but full sequence not completed | Global timeout after `UNLOAD_TIMEOUT` ms. | Feeder queue flushed. `CMD_FULL_UNLOAD` issued. State machine resets. |

#### 6.3 Emergency Stop / User Override

- If the user presses the printer's physical stop button during the dwell, the printer immediately aborts the filament change.
- The microcontroller detects the halt (toolhead stops moving) and resets its state machine, waiting for the next filament change cycle.

---

### 7. Future Expansion

While the current implementation supports up to 20 slots (indices 0–19), the protocol is designed to be easily scalable.

#### 7.1 Extending Slot Count (Up to 29 Slots)

The G-code already contains a commented-out section for 29-slot operation:

```gcode
; Uncomment for VL53L1X (29 slots — not yet tested):
; {if next_extruder >= 0 && next_extruder <= 28}
; G1 X{-19 + (next_extruder * 10)} F18000
; M400 S2
; {else}
; M400 U1
; {endif}
```

**To expand to 29 slots:**
- Change the upper bound from `19` to `28`.
- The X-coordinate range becomes: `X = -19` to `X = 261` (total span: 280 mm).
- This fits within the Bambu Lab A1's physical X-axis travel.
- A minor adjustment to the offset (`-19`) or step size (`×10`) may be required depending on hardware revision.

#### 7.2 Theoretical Maximum Slot Count

While the current implementation supports up to 20 slots reliably and can be extended to 29 with VL53L1X (theoretically), the theoretical maximum is **32 slots**.  
Achieving this requires a distance sensor with finer millimeter-level resolution and stability than the VL53L0X.  
Without higher accuracy, tolerance bands would overlap at the extreme ends of travel, making reliable decoding impossible.

---

### 8. Licensing Summary

This document is governed by a **different license** from the rest of the EFAC-A1 project. The table below summarizes the full project licensing:

| Component | License | Rationale |
| :--- | :--- | :--- |
| **This PESA Protocol Specification** | CC0 1.0 Universal (Public Domain) | Dedicated to the public domain to serve as unrestricted prior art. |
| **Firmware** (ESP32/Arduino code) | AGPL-3.0 | Strong copyleft ensures all modifications remain open source. |
| **G-code** (Bambu Studio macros) | MIT | Permissive; derivative work of open-source G-code by Andrzej Leszkiewicz and Steven Wu. |
| **3D Models & Schematics** | CERN-OHL-S v2 | Strongly reciprocal hardware license; modifications must be shared under the same terms. |

---

### Version History

| Version | Date | Author | Changes |
| :--- | :--- | :--- | :--- |
| 1.0 | 2026-07-06 | Hans930v (Hansoy) | Initial public release. Defensive publication. |
| 1.1 | 2026-07-06 | Hans930v (Hansoy) | Corrected hardware topology to include hall-effect sensor as non-contact arming signal. Updated Overview, Section 2, Section 4 intro, Section 4.3, Section 5, and Section 6.2 to accurately reflect two-sensor architecture and event-driven state machine. |

---

*End of Specification.*
