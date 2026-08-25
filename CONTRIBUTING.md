# Contributing to EasySteel

Thank you for contributing to EasySteel. This repository contains the standalone ESP32 firmware for the timer and target controller. The device remains authoritative for round state, timing, hits, penalties, confirmation, and reset.

## Before you start

- Discuss larger firmware, protocol, or hardware changes in an issue before implementing them.
- Do not add desktop or network commands that can start, stop, reset, correct, or otherwise override a round.
- Treat the hardware documentation as safety-relevant. Do not present an untested wiring change as a verified reference design.
- Never commit credentials, Wi-Fi details, private IP addresses, device identifiers, or machine-local PlatformIO configuration.
- By contributing, you confirm that you have the right to license the contribution under the path-specific terms in [LICENSES.md](LICENSES.md).

## Development workflow

1. Create a focused branch and keep unrelated changes out of the pull request.
2. Use English for code, comments, identifiers, tests, and commit messages.
3. Use Conventional Commit messages, for example `fix(firmware): preserve hit order`.
4. Add or update native regression tests for every behavioral change.
5. Update public documentation whenever behavior, protocol, configuration, or wiring changes.

Run these checks from the repository root before opening a pull request:

```powershell
pio test -e native
pio run -e esp32dev
pio check -e esp32dev --skip-packages
```

Hardware changes additionally require a review against [docs/HARDWARE.md](docs/HARDWARE.md), including the commissioning checks. Test results from a real reference device must state the hardware revision and the exact components used.

## Pull request checklist

- [ ] The change has a narrow, described purpose.
- [ ] Tests cover changed behavior and all required checks pass.
- [ ] Documentation and protocol examples match the implementation.
- [ ] No generated files, firmware binaries, or local configuration are included.
- [ ] The ESP32 remains the leading system for the round lifecycle.
- [ ] Hardware claims are backed by a tested reference setup.

## Reporting problems

Use the issue templates for reproducible bugs and feature proposals. For security-sensitive issues, follow [SECURITY.md](SECURITY.md) and do not open a public issue.
