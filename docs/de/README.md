# EasySteel Firmware-Dokumentation

> 🌐 Sprache: [🇬🇧 English](../README.md) | **🇩🇪 Deutsch**

Willkommen in der Dokumentation der eigenständigen EasySteel ESP32-Firmware.

## Dokumentenübersicht

- [**Firmware-Architektur & Bedienung**](FIRMWARE.md) — Runden-Zustandsmaschine, Tasterfunktionen, Schießsportmodi, OLED-Display und OTA-Updates.
- [**Hardware-Spezifikation & Inbetriebnahme**](HARDWARE.md) — Vollständiger Schaltplan, ESP32-Pinbelegung, Pegelwandler (TXS0108E), LED-Treiber (ULN2803A), RJ45-Feldverkabelung und elektrische Vorab-Prüfungen.
- [**TCP-Protokoll v1**](PROTOCOL.md) — Integrationsvertrag zwischen ESP32 und externer Desktop-Anwendung (JSON-Ereignisse, Capability-Aushandlung und zuverlässige Ergebnisquittierung).
- [**System-Autorität & Grenzen**](ARCHITECTURE.md) — Das Hardware-First-Prinzip und Verantwortlichkeiten zwischen ESP32 und PC.
- [**Projekt-Roadmap**](ROADMAP.md) — Nächste Meilensteine, Prioritäten und Entwicklungsschritte.
- [**Release-Prozess**](RELEASES.md) — Semantische Versionierung, Prüfsummen und Build-Checkliste.

---

> ℹ️ Die Windows-Desktop-Anwendung ist ein separates Projekt. Verweise darauf beschreiben die externe TCP-Schnittstelle, keinen im Repository enthaltenen Code.
