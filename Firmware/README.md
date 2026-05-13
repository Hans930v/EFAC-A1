# EFAC-A1 Firmware

> ⚠️ **Work In Progress** — Firmware files will be published here upon the official v1.0.0 release.

## Overview

The EFAC-A1 firmware is built on the **ESP32 platform** using **PlatformIO + VS Code** as the development environment. It is written in C++ and structured as a modular, multi-file project — not a single sketch.

The firmware is responsible for:

- Interpreting filament slot commands received via the custom EFAC G-code
- Controlling the feeder state machine
- Managing servo indexing for gear engagement/disengagement per slot
- Driving DC motors via motor driver modules for filament push/pull
- Reading filament presence sensors and slot interpretations
- Handling OLED UI feedback
- Delivering **OTA (Over-the-Air) firmware updates via Wi-Fi** — this is the sole purpose of Wi-Fi connectivity in EFAC-A1. No data is collected, transmitted, or stored externally.

## Why the source isn't here yet

EFAC-A1 is still in active development. The mechanical system is being finalized and the firmware is being validated against real hardware before a stable release is published. Releasing incomplete firmware prematurely risks misleading builders or causing hardware damage.

**The full source will be released at v1.0.0** — open, documented, and ready to build.

## Privacy

EFAC-A1 uses Wi-Fi **exclusively** for OTA firmware updates. It does not phone home, collect data, or require any cloud account. The source code will be fully open so anyone can verify this themselves.

## License

AGPL-3.0 — see the root [LICENSE](../LICENSE) file.
