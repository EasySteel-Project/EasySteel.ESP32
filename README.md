# EasySteel ESP32 Firmware

EasySteel is the standalone firmware for an ESP32-based Steel Challenge timer and target controller. The Windows match-management application is maintained separately and communicates with the device through newline-terminated JSON over TCP.

The ESP32 is the authoritative system for round state, timing, hits, penalties, confirmation, and reset. A connected desktop application may display and persist device events and provide match context, but it must not control or override a round. The timer remains fully usable without a PC connection. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Features

- Five interrupt-driven target inputs with debounce and global crosstalk suppression
- Competition, all-targets training, and reaction drill modes
- Physical start, stop, and reset controls for range officers
- SH1106 OLED, target LEDs, status LED, and non-blocking buzzer patterns
- Wi-Fi provisioning, offline operation, WPS, and authenticated OTA updates
- Non-blocking TCP delivery with a bounded static live-event queue and retained final-result outbox
- Native PlatformIO tests for timing drivers, storage, result handling, buffering, and protocol serialization

## Repository layout

```text
.
├── include/            Public headers grouped by core, hardware, and network
├── src/                Firmware implementations and Arduino entry point
├── test/               Native Unity tests and hardware mocks
├── docs/               Architecture, hardware, firmware, and release documentation
├── platformio.ini      Pinned build and test environments
└── extra_script.py     PlatformIO build metadata helper
```

Documentation:

- **Firmware behavior and operation:** [docs/FIRMWARE.md](docs/FIRMWARE.md)
- **Hardware specification and wiring:** [docs/HARDWARE.md](docs/HARDWARE.md)
- **TCP Protocol v1:** [docs/PROTOCOL.md](docs/PROTOCOL.md)
- **System authority and boundaries:** [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- **Release process:** [docs/RELEASES.md](docs/RELEASES.md)
- [Licensing](LICENSES.md)
- [EasySteel name and logo policy](TRADEMARKS.md)
- [Third-party notices and audit status](THIRD_PARTY_NOTICES.md)

## Project status and contribution

EasySteel is in pre-release development. It is not a safety interlock and must not be the sole safety measure on a shooting range. Firmware, documentation, and future hardware design source use the copyleft licensing model described in [LICENSES.md](LICENSES.md). The complete manufacturing-ready hardware-design package is still pending before the first public release.

Contribution and security-reporting rules are documented in [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).

## Supported reference hardware

- ESP32 DevKit/NodeMCU-compatible 30-pin board using the pin map in `include/core/config.h`
- Five LM393-based target sensor modules through 5 V to 3.3 V level conversion
- SH1106 128×64 I2C OLED at address `0x3C`
- Three active-low physical buttons
- ULN2803A-driven target LEDs and active 5 V buzzer

The exact electrical design and mandatory commissioning checks are defined in [docs/HARDWARE.md](docs/HARDWARE.md). Do not build from an older simplified wiring image.

## Build and test

Prerequisites are Python 3.12 and PlatformIO 6.1.18:

```powershell
python -m pip install "platformio==6.1.18"
```

Run from the repository root:

```powershell
pio test -e native
pio run -e esp32dev
pio check -e esp32dev --skip-packages
```

Upload over USB with `pio run -e esp32dev -t upload`. The resulting browser-OTA image is `.pio/build/esp32dev/firmware.bin`.

Machine-specific ports and OTA addresses belong in the ignored `platformio.local.ini`:

```ini
[env:esp32dev]
upload_port = COM7
monitor_port = COM7

[env:esp32dev_ota]
upload_port = 192.168.0.101
```

## Result lifecycle

The device enters `FINISHED` before submitting a result so the range officer can review splits and penalties:

- Reset short press: confirm and submit exactly one `end` event
- Reset long press: discard the result and submit `reset`
- Reaction Drill start press: confirm the displayed result and prepare the next attempt

## TCP protocol

Firmware events are UTF-8 JSON objects terminated by `\n`. Protocol version 1 includes `protocolVersion` on every event and `runId` on final results.

The complete field, event, compatibility, and delivery contract is documented in [docs/PROTOCOL.md](docs/PROTOCOL.md).

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"end","timeMs":4250,"targetCount":5,"runId":"91af20c3-7","rawTimeMs":1250,"missCount":1,"penaltyCount":1,"penaltyTimeMs":3000}
```

Every protocol v1 server must reply within one second after connection:

```json
{"protocolVersion":1,"event":"capabilities","resultAck":true}
```

The firmware retains each result in its RAM outbox until acknowledgement:

```json
{"protocolVersion":1,"event":"ack","runId":"91af20c3-7"}
```

Missing or incompatible capability negotiation closes the connection; queued results remain available for the next reconnect. Desktop applications should ignore unknown JSON properties.

The result outbox holds at most 20 entries and is intentionally RAM-only. Missing acknowledgements trigger retransmission after three seconds. A reboot or power loss removes results that have not yet been delivered.

Capabilities and acknowledgements affect delivery only. They never transfer round control to the server.

The round controller only appends serialized messages to a bounded static queue. The network tick performs at most one non-blocking socket write per iteration, so an unresponsive desktop application cannot stall target processing or trigger the task watchdog. Final results remain in the separate outbox until the matching acknowledgement arrives.

## Web and OTA security

- The provisioning portal is available only on the password-protected device access point.
- Browser OTA is disabled during provisioning.
- Settings and browser OTA in station mode use HTTP Basic authentication: user `admin` and the device password shown during provisioning.
- OTA is processed only while the round controller is idle.
- OTA failure and abort paths restore the watchdog state.

Basic authentication is intended for a trusted range LAN. Signed firmware is recommended for deployments with stronger security requirements.

## Development rules

- English for code, comments, identifiers, and commits
- Conventional Commits
- No heap allocation or blocking delays in active timing loops
- Keep ISRs short and use the chronological static hit queue
- Add or update native tests for behavioral changes
