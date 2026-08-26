# EasySteel - Roadmap

> 🌐 Sprache: [🇬🇧 English](../ROADMAP.md) | **🇩🇪 Deutsch**

## Architektur-Leitplanke

Der ESP32 ist das führende System. Er steuert den gesamten Rundenablauf und ist die alleinige Quelle für Zustand, Zeitmessung, Treffer, Strafen, offizielle Ergebnisbestätigung und Reset. Die Windows-Anwendung zeigt Ereignisse an, speichert Ergebnisse und verwaltet Schützen und Turniere. Ein physisch freigegebenes und gekoppeltes Handy darf begrenzte Eingabeanforderungen für das Einzeltraining senden; jede Zustandsänderung wird jedoch vom ESP32 geprüft. Ein Verbindungsabbruch darf die Hardware nicht am Betrieb hindern.

## Hohe Priorität

### 1. Protokoll v1 stabilisieren

- [x] Ereignisse, Pflichtfelder, Einheiten und Zustellklassen als versionierten Vertrag dokumentieren.
- [x] Strenge Parser-, Framing-, Validierungs- und Pre-v1-Ablehnungstests mit realistischen Nachrichten ergänzen.
- [ ] Wiederverbindung, doppelte Ergebnisse und verlorene ACKs als Integrationstest abdecken.
- [x] Klarstellen: ACK bestätigt nur den Empfang und niemals einen Zustandswechsel der Hardware.

### 2. Schützen- und Match-Kontext anzeigen

- [x] Das nicht steuernde Ereignis `match_info` mit opaker `contextId` zur Zuordnung definieren.
- [x] Strenge Bytegrenzen und druckbare ASCII-Zeichen für Geräte-Anzeigetext durchsetzen.
- [ ] Schütze, Stage und Durchgang im Standby anzeigen.
- [x] Kontext außerhalb von Standby sperren und sicherstellen, dass er Timing oder Wertung nie beeinflusst.

Nicht vorgesehen sind PC-Befehle für `arm`, `disarm`, `start`, `reset`, Zeitkorrektur oder Trefferänderung. Die getrennte, gekoppelte Einzeltrainings-Oberfläche unterstützt ausschließlich Start, Stop, Reset und die Moduswahl in Standby und erzeugt niemals offizielle Ergebniszustellungen.

### 3. Hardwarebetrieb absichern

- [x] Physisch freigegebenes vierstelliges Pairing für genau ein Einzeltrainings-Handy ergänzen.
- [x] Priorität Hardware > Desktop > Handy durchsetzen und Handy-Steuerung bei Desktop-Verbindung widerrufen.
- [x] Stabile, nicht autoritative Trainingsresultat-IDs für genau einmalige LocalStorage-Historie bereitstellen.
- [ ] Stromausfall, WLAN-Ausfall und Server-Neustart in allen Rundenphasen testen.
- [ ] Sichtbare Gerätewarnung bei vollem RAM-Outbox-Puffer festlegen; serieller Hinweis und Ersetzen des ältesten Ergebnisses sind implementiert.
- [ ] Hardware-in-the-loop-Testplan für Taster, Sensorreihenfolge und Zeitmessung erstellen.

## Open-Source-Vorbereitung

- [x] Copyleft-Lizenzmodell festlegen: GPL-3.0-or-later für Firmware, CERN-OHL-S-2.0 für künftige Hardware-Designquellen und CC-BY-SA-4.0 für Dokumentation.
- [ ] Reproduzierbare Releases mit Quellcode, Firmware-Binärdatei und Prüfsummen erstellen.
- [ ] Bauanleitung, Schaltplan, Stückliste, Gehäuse, Kalibrierung und Sicherheitshinweise veröffentlichungsreif machen.
- [x] `CONTRIBUTING.md`, `SECURITY.md`, Issue-Vorlagen und Versionsstrategie ergänzen.
- [ ] Drittanbieter-Lizenzen und -Hinweise vor dem ersten Release vollständig auditieren; direkte Firmware-Bibliotheken sind inventarisiert, die Framework- und Funkarchiv-Nachweise (SBOM/Notices) bleiben offen.
- [x] EasySteel-Name und -Logo von den urheberrechtlichen Lizenzen abgrenzen.

## Niedrige Priorität / Evaluierung

### Kabellose Zielsensoren per ESP-NOW

- [ ] Zielmodule mit ESP32-C3, Piezo-Sensor, Akku und geschütztem Gehäuse evaluieren.
- [ ] Latenz, Paketverlust und Kollisionsverhalten unter realen Wettkampfbedingungen messen.
- [ ] Akkulaufzeit, LED-Stromversorgung und Trefferschutz gegen die bestehende Kabellösung abwägen.
