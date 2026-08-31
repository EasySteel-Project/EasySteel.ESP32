# EasySteel Firmware Documentation

Welcome to the technical documentation for the standalone EasySteel ESP32 target and timer firmware.

## Documentation Index

- [**Firmware Architecture & Operation**](FIRMWARE.md) — Round state machine, button interactions, match modes, OLED display, and OTA updates.
- [**Hardware Specification & Commissioning**](HARDWARE.md) — Circuit diagrams, ESP32 pinout, level shifter (CD4050BE), LED driver (ULN2803A), RJ45 field cabling, and mandatory electrical safety checks.
- [**TCP Protocol v1**](PROTOCOL.md) — Integration contract between ESP32 and external desktop applications (JSON events, capability negotiation, and reliable result acknowledgement).
- [**System Authority & Boundaries**](ARCHITECTURE.md) — Hardware-first design principle and distribution of responsibilities between ESP32 and PC.
- [**Release Process**](RELEASES.md) — Semantic versioning, reproducible builds, and release checklists.

---

> ℹ️ The Windows desktop application is maintained separately. References to it describe the external TCP integration contract, not code contained in this repository.
