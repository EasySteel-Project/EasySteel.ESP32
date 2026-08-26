# EasySteel - Roadmap

> 🌐 Language: **English** | [🇩🇪 Deutsch](de/ROADMAP.md)

## Core Architectural Guardrail

The ESP32 is the authoritative system. It manages the entire round lifecycle and is the sole source of truth for round state, timing, hits, penalties, result confirmation, and reset. The Windows desktop application observes events, stores confirmed scores, and manages shooters and tournament schedules. Network disconnection must never impede hardware timing operations.

## High Priority

### 1. Protocol v1 Stabilization

- [x] Document events, mandatory fields, units, and delivery classes as a versioned specification.
- [x] Add strict parser, framing, validation, and pre-v1 rejection tests with realistic payloads.
- [ ] Cover reconnection, duplicate results, and dropped ACKs with integration tests.
- [x] Clarify: ACK confirms transport receipt only and never triggers a hardware state transition.

### 2. Shooter & Match Context Display

- [x] Define the non-controlling `match_info` event with opaque `contextId` correlation.
- [x] Enforce strict byte limits and printable-ASCII validation for device display text.
- [ ] Render shooter, stage, and string on the OLED standby screen.
- [x] Lock context outside Standby and guarantee that it never alters round timing or scoring.

Commands for remote `arm`, `disarm`, `start`, `reset`, time correction, or penalty modification remain explicitly excluded.

### 3. Operational Reliability

- [ ] Test power loss, Wi-Fi outage, and server crashes during all round states.
- [ ] Define visual OLED warning for full RAM outbox (serial warning and oldest-drop are implemented).
- [ ] Establish hardware-in-the-loop test procedures for buttons, sensor ordering, and timing accuracy.

## Open-Source Preparation

- [x] Define copyleft licensing: GPL-3.0-or-later for firmware, CERN-OHL-S-2.0 for future hardware designs, CC-BY-SA-4.0 for documentation.
- [ ] Establish reproducible release builds with source tags, firmware binaries, and SHA-256 checksums.
- [ ] Finalize build manuals, schematics, BOM, 3D enclosures, calibration, and electrical safety guides for public release.
- [x] Add `CONTRIBUTING.md`, `SECURITY.md`, issue templates, and versioning guidelines.
- [ ] Complete third-party licensing audit prior to binary release (direct libraries audited; framework/radio archives tracking in `THIRD_PARTY_NOTICES.md`).
- [x] Establish trademark and logo policy in `TRADEMARKS.md`.

## Future Concepts / Evaluation

### Wireless Target Sensors via ESP-NOW

- [ ] Evaluate wireless target nodes using ESP32-C3, piezo sensors, batteries, and ballistic enclosures.
- [ ] Benchmark ESP-NOW latency, packet loss, and collision behavior under live match conditions.
- [ ] Assess battery runtime, LED power requirements, and armor shielding against the existing wired RJ45 solution.
