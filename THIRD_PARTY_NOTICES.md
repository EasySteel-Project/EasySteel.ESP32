# EasySteel Third-Party Notices

This document records third-party components included in the ESP32 firmware build. Include this file, the applicable licence texts, and the matching EasySteel source tag with every binary release.

## Audit scope

The inventory below was derived from the reproducible `esp32dev` build defined in `platformio.ini`:

- Platform: `espressif32@7.0.1`
- Framework package: `framework-arduinoespressif32@3.20017.241212+sha.dcc1105b`
- Firmware map: `.pio/build/esp32dev/firmware.map`

The map confirms that the four libraries marked as included below contribute code to `firmware.bin`. Native Unity tests, PlatformIO itself, the compiler, and static-analysis tools are development tooling and are not release artifacts.

## Included runtime components

| Component | Resolved version | Licence | Upstream source | Release attribution |
|---|---:|---|---|---|
| Arduino-ESP32 framework | `3.20017.241212+sha.dcc1105b` (Arduino core `2.0.17`, ESP-IDF `4.4.7`) | LGPL-2.1-or-later (package metadata) | <https://github.com/espressif/arduino-esp32/tree/2.0.17> | Preserve the framework's licence and notices; provide the matching EasySteel source. |
| Adafruit GFX Library | `1.12.6` | BSD | <https://github.com/adafruit/Adafruit-GFX-Library> | Include its BSD licence text and required Adafruit copyright notice. |
| Adafruit SH110X | `2.1.15` | BSD | <https://github.com/adafruit/Adafruit_SH110X> | Include its BSD licence text and required Adafruit copyright notice. |
| Adafruit BusIO | `1.17.4` | MIT | <https://github.com/adafruit/Adafruit_BusIO> | Include its MIT licence text and copyright notice. |
| ArduinoJson | `7.4.3` | MIT | <https://github.com/bblanchon/ArduinoJson> | Include its MIT licence text and copyright notice. |

Adafruit BusIO is transitive: it is required by the display stack and is present in the linked firmware even though it is not listed directly in `platformio.ini`.

## Direct-library compatibility result

The direct library inventory contains LGPL-2.1-or-later, BSD, and MIT components. No direct library listed above prevents distributing EasySteel under GPL-3.0-or-later. This statement does not cover the complete framework closure described below. It is an attribution and compatibility assessment, not legal advice.

## Framework closure: unresolved

The firmware map also links prebuilt Espressif radio archives, including `libnet80211.a` and `libwpa_supplicant.a`, from the Arduino-ESP32 framework package. Arduino core `2.0.17` is based on ESP-IDF `4.4.7`; the version-matched [ESP-IDF copyright page](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/COPYRIGHT.html) identifies `wpa_supplicant` and FreeBSD `net80211` as BSD-licensed. The installed PlatformIO framework package still does not provide a component-level software bill of materials or a complete notice bundle for every linked archive.

Consequently, this audit makes **no full-binary licence-compatibility claim** yet. The framework and radio-archive licence/notice evidence must be completed before any public `firmware.bin` release.

## Remaining release blocker

`framework-arduinoespressif32` packages Arduino-ESP32, ESP-IDF components, and prebuilt radio archives. Its package-level metadata identifies LGPL-2.1-or-later, but that is not a complete software bill of materials for every bundled component.

Before the first public binary release, the maintainer must:

1. generate or obtain the framework's version-matched component-level licence inventory;
2. preserve every applicable notice and licence text in the release bundle;
3. verify that the published EasySteel source tag, build configuration, and dependency versions reproduce the released binary;
4. review any newly added library, font, image, copied snippet, or hardware asset before it enters a release.

Until these four points are completed, the third-party attribution audit is intentionally incomplete and a binary release must remain a draft.
