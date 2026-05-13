# EFAC-A1 Hardware

> ⚠️ **Work In Progress** — Hardware files will be published here upon the official v1.0.0 release.

## Overview

This folder will contain all printable and mechanical files needed to build EFAC-A1, including STL files, CAD source files, and assembly references.

EFAC-A1 is designed with the following hardware philosophy:

- **Print it yourself** — all structural parts are designed for FDM printing in standard PLA. No exotic materials required.
- **Designed with repairability in mind** — the goal is for components to be individually replaceable rather than requiring full system replacement. Final modularity details will be confirmed at v1.0.0.
- **Scale it yourself** — the base unit supports 4 colors. Each expansion unit adds 4 more. The architecture supports up to 32 colors theoretically.

## What will be included

- STL files for all printed parts (feeder body, gear housing, lever arm, mounts)
- CAD source files (STEP)
- Bill of Materials (BOM) with part counts per unit
- Assembly reference images

## Design Notes

All parts are dimensionally validated against physical hardware before being released. Tolerances are tuned for standard FDM printing — no supports required where avoidable, and critical fit dimensions are verified through iterative test prints.

The engagement/disengagement mechanism uses an **MG90S micro servo** per pair of feeder slots, deliberately chosen to keep cost and bulk minimal. The 25GA-370 geared DC motors handle filament drive per slot.

## Print Settings (preliminary)

| Setting | Value |
|---|---|
| Material | PLA |
| Layer Height | 0.2mm |
| Infill | 20–40% depending on part |
| Supports | Part-dependent (noted per STL) |

## License

CERN-OHL-S-2.0 — see the root [LICENSE](https://github.com/Hans930v/EFAC-A1/blob/main/Hardware/LICENSE_HARDWARE) file.
