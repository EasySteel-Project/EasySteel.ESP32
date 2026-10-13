# Release Process

This document defines the release process for the standalone EasySteel firmware. It is intended to make source, firmware binary, and checksums traceable and reproducible.

## Versioning

Firmware releases use Semantic Versioning with `v`-prefixed Git tags, for example `v0.1.0`.

- Patch releases correct compatible defects.
- Minor releases add backward-compatible features.
- Major releases may contain incompatible configuration, protocol, or hardware changes.
- Protocol v1 remains compatible for every release that advertises `protocolVersion: 1`. An incompatible protocol change requires a new `protocolVersion`.

## Release checklist

1. Confirm that the working tree is clean and the release commit is reviewed.
2. Run the complete verification suite:

   ```powershell
   pio test -e native
   node --test test/web_training_history.test.cjs
   pio run -e esp32dev
   pio check -e esp32dev --skip-packages
   ```

3. Flash `.pio/build/esp32dev/firmware.bin` to the reference device and verify boot, display, inputs, target detection, result confirmation, Wi-Fi provisioning, and OTA authentication.
4. Copy the binary to a versioned release asset and create its SHA-256 checksum:

   ```powershell
   Copy-Item .pio/build/esp32dev/firmware.bin EasySteel-firmware-vX.Y.Z.bin
   Get-FileHash EasySteel-firmware-vX.Y.Z.bin -Algorithm SHA256 |
     Format-List Algorithm,Hash,Path
   ```

5. Create and push the signed or annotated tag `vX.Y.Z` only after the checks pass.
6. Publish a GitHub Release containing:

   - the source tag;
   - `EasySteel-firmware-vX.Y.Z.bin`;
   - a `SHA256SUMS.txt` file containing the checksum;
   - [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) and all applicable third-party licence texts;
   - concise release notes, including known limitations and hardware compatibility.

7. Keep the release as a draft until the licence notices, third-party attribution audit, security reporting channel, and hardware documentation are ready.

## Release artifacts

Generated binaries and checksums are release artifacts. They must not be committed to the source repository. The exact PlatformIO and dependency versions are pinned in [platformio.ini](../platformio.ini).

The release notes must link to [LICENSES.md](../LICENSES.md), preserve third-party notices, and state whether the release contains a manufacturing-ready hardware-design package. EasySteel name and logo usage is governed separately by [TRADEMARKS.md](../TRADEMARKS.md).
