# EasySteel Licensing

Copyright (c) 2026 David Struthers.

Unless a file contains a more specific notice, original EasySteel material is licensed according to the following file-scope rules. Third-party material retains its own copyright notices and licence terms.

## Firmware

The original source and build material in these paths is licensed under the GNU General Public License, version 3 or later:

- `src/`
- `include/`
- `test/`
- `extra_script.py`
- `platformio.ini`
- `.github/workflows/`

SPDX identifier: `GPL-3.0-or-later`

The licence permits commercial use, modification, and distribution. A distributor of modified firmware or firmware binaries must meet the GPL's corresponding-source obligations. The canonical licence text is published by the Free Software Foundation at <https://www.gnu.org/licenses/gpl-3.0.html>.

## Hardware design source

Future released design source in `hardware/` — for example schematics, PCB layout, CAD, manufacturing files, and design-specific BOMs — is intended to be licensed under CERN Open Hardware Licence Version 2 - Strongly Reciprocal.

SPDX identifier: `CERN-OHL-S-2.0`

The first released hardware design revision must contain a Source Location notice and the complete preferred design source. The canonical licence text is maintained by CERN/Open Hardware Repository at <https://ohwr.org/cern_ohl_s_v2.txt>.

No complete hardware design source is published in this repository yet. [docs/HARDWARE.md](docs/HARDWARE.md) is reference documentation, not a manufacturing-ready design package.

## Documentation and project materials

The original documentation and project-management material in these paths is licensed under Creative Commons Attribution-ShareAlike 4.0 International:

- `README.md`
- `docs/`
- `CONTRIBUTING.md`
- `SECURITY.md`
- `LICENSES.md`
- `TRADEMARKS.md`
- `.github/ISSUE_TEMPLATE/`

SPDX identifier: `CC-BY-SA-4.0`

The licence permits commercial reuse with attribution and requires adapted material to be shared under the same or a compatible licence. The canonical legal code is available at <https://creativecommons.org/licenses/by-sa/4.0/legalcode.en>.

## Contributions

By submitting a contribution, each contributor confirms that they have the right to submit it and licenses it under the licence that applies to the affected path. Contributors retain their copyright.

## Third-party components

The PlatformIO dependencies listed in `platformio.ini`, ESP32 framework components, and externally sourced assets are not relicensed by EasySteel. Their notices and licence obligations must be preserved in every firmware release. The current inventory and remaining release blocker are recorded in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). A release must not be published until this attribution audit is complete.

## Trademarks

The copyright licences above do not grant permission to use the EasySteel name or any EasySteel logo as a trademark. See [TRADEMARKS.md](TRADEMARKS.md).
