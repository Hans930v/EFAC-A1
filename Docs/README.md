# PESA (Position-Encoded Slot Assignment)
## Communication Protocol Specification v1.0

**Author:** Hans930v (Hansoy)  
**Date of Publication:** 2026-07-06  
**Project:** EFAC-A1 (External Feeder–Assisted Filament Change for Bambu Lab A1)  

---

### Legal Notice & License

This document is a **defensive publication** intended to establish prior art. 
It is dedicated to the **public domain** under the **CC0 1.0 Universal (CC0 1.0) Public Domain Dedication**. 

To the fullest extent permitted by law, the author waives all copyright and related rights to this specification. This ensures that any patent office, examiner, or third party may freely use this document to reject any later patent claims covering the methods described herein.

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
