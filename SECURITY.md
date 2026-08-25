# Security Policy

## Supported versions

Before the first public release, security fixes are made on the current `main` branch. A release support matrix will be added with the first versioned firmware release.

## Reporting a vulnerability

Do not report vulnerabilities in a public issue. Before the first public release, maintainers must enable GitHub private vulnerability reporting for the repository. Use that channel for reports affecting, for example:

- browser OTA and authentication;
- Wi-Fi provisioning or stored credentials;
- TCP input parsing and result delivery;
- firmware update integrity;
- unintended remote influence on the device-owned round lifecycle.

If private vulnerability reporting is not available yet, contact a repository maintainer privately through the repository hosting platform. Do not include passwords, Wi-Fi names, private IP addresses, device identifiers, or full production logs in a public report.

## Scope and safety

EasySteel is a timing and target-controller project. It is not a safety interlock and must not be the sole control for a shooting range or any situation where a malfunction could cause injury. Follow the commissioning and electrical safety checks in [docs/HARDWARE.md](docs/HARDWARE.md) before field use.
