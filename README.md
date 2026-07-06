# Work In Progress (WIP) - External Feeder-Assisted filament Change for Bambu Lab A1 (EFAC-A1)

> **NOTE**: This project is currently a **Work In Progress**. Some features are not yet implemented or require further testing. Please proceed with caution and ensure you understand the setup before implementation. 

EFAC-A1 enables an **AMS-like** filament change workflow using an external feeder, without modifying the printer’s firmware or using the AMS port. This system relies on a **custom Change Filament G-code** to communicate with the external feeder, making it a software-safe alternative to AMS. It supports multi-filament setups and works seamlessly with Bambu Studio.

## Acknowledgments

A big thanks to [**3Dnaut(MakerWorld)**](https://makerworld.com/en/models/1534741-multi-color-without-ams-with-ams-test#profileId-1609872)/[**Andrzej Leszkiewicz(GitHub)**](https://github.com/avatorl/bambu-a1-g-code) for providing the starting point of the custom G‑code used in EFAC‑A1. 
Additional appreciation goes to [**MrMonkey**](https://github.com/avatorl/bambu-a1-g-code/pull/17) for his PR introducing the current‑boost idea, which has been integrated into EFAC-A1's G-code.
This project builds on those foundations and continues to evolve through community contributions.


## Why EFAC-A1?
- The Bambu Lab AMS is great, but not everyone can afford one.
- EFAC mimics AMS behavior using external hardware or manual swaps.
- **Firmware‑safe**: uses official AMS flush logic, so slicer reports stay accurate.
- **Tested**: slicer said 8.99 g, actual model & waste weighed 9.00g (only 0.01 g difference!)
- **Scalable**: Base unit supports 4 colors, expandable in sets of 4 via stacking units — currently supports up to 16 colors, with a theoretical maximum of 32.

## Status & Roadmap
> G-code is finalized and working. Hardware and firmware are still being validated.
> Version will bump to 1.0.0 once hardware testing is complete.

| Milestone | Status |
|---|---|
| G-code logic | Final (v0.9.7) |
| Firmware (PrinterComms + Feeder) | In Progress |
| Hardware design | In Progress |
| Schematics | In Progress |
| Real hardware validation | In Progress |
| 1.0.0 release | Pending |

## How It Works
EFAC-A1 uses **PESA** (Position-Encoded Slot Assignment) — its own communication protocol — to coordinate between the printer and external feeder without any firmware modification. See the PESA section below for full details.

## PESA — Position-Encoded Slot Assignment

PESA is the communication protocol at the heart of EFAC-A1. From the Filipino word *"pyesa"* meaning part, PESA is the part that makes EFAC-A1 work.

### How PESA Works
The printer moves its toolhead to a specific X-axis position encoded from the slicer's native `NEXT_EXTRUDER` variable using a simple linear formula: X = -19 + (next_extruder × 10mm)

A VL53L0X Time-of-Flight sensor reads that position as a distance, decodes it as a slot number, and triggers the external feeder to load the correct filament — all without touching Bambu Lab's proprietary firmware or hardware.

### Why PESA is Unique
Every other open-source multi-material system targets open-source printers where firmware is accessible. PESA is designed specifically for a closed-firmware printer — turning the printer's own motion into a unidirectional communication channel. No reverse engineering, no proprietary protocols, no firmware modifications. Just movement.

> This communication method is believed to be novel. Public documentation serves as prior art.

## Slicer Estimates vs Actual Weighing
### Bambu Studio: <img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/2c704cb0-1f68-4d7f-addb-1aa7ff3d6b10" />

### Actual: <img width="1080" height="1383" alt="image" src="https://github.com/user-attachments/assets/0e4348db-0144-410f-b911-c2487b44f231" />

## Change Filament G-codes
> **Disclaimer**: This is **NOT** official Bambu Lab G‑code.  
> These are community‑developed, experimental files intended for use with EFAC‑A1 hardware.

EFAC-A1 G-code: [**Change Filament Gcode for EFAC-A1**](https://github.com/Hans930v/EFAC-A1/blob/main/G-code/EFAC-A1-Gcode.gcode)

## Features
- **Software-Safe**: No printer firmware modifications required.
- **External Feeder**: Works with an external filament feeder to enable AMS-like behavior.
- **Multi-Filament Support**: Easily handle multiple filaments for diverse printing needs.
- **Compatible with Bambu Studio**: Fully compatible with Bambu Studio for filament changes.
- **PESA Protocol**: A communication method that uses the printer's own X-axis motion as a data channel — no proprietary protocol access required.


### Step‑by‑Step Process

1. **G‑code Workflow**  
   Copy the provided G‑code file for EFAC‑A1.  

2. **External Feeder Setup**  
   Install the external feeder and position the sensors in their designated locations on the printer.

3. **Filament Change**  
   The G‑code manages unloading and flushing. EFAC then handles loading the new filament, replicating AMS‑style behavior without firmware modifications.

4. **Flush Volumes**  
   Automatic purging ensures clean filament transitions, with flush volumes calibrated for reliable multi‑color printing.

---

# License
- EFAC‑A1 Code & Firmware → GNU General Public License v3.0 
- Change Filament G‑code → MIT License (see [/G-code/LICENSE](/G-code/LICENSE))  
- 3D Models, CAD assets, & Schematics → CERN Open Hardware Licence Version 2 - Strongly Reciprocal (see [/Hardware/LICENSE](/Hardware/LICENSE))  

See [NOTICE.md](NOTICE.md) for full details.

# Disclaimer
> The PESA communication method is documented here as public prior art (first documented: 2026). 
> This prevents third-party patents on the method while keeping it open for everyone.

EFAC‑A1 is an **UNOFFICIAL**, community‑driven project and is **NOT** affiliated with Bambu Lab. It does **NOT** replace the official AMS — think of it as a community-built companion for those who want multi-color printing without the AMS price tag.

That said, this is a **user-driven** solution that requires **MANUAL SETUP**, hardware assembly, EFAC firmware, and **CUSTOM G-codes** — so please read through everything before diving in, and test thoroughly before relying on it for real prints. All modifications are done at your own risk.

If you run into issues or have questions, feel free to open a GitHub Issue — happy to help where I can. 

Enjoy building!
