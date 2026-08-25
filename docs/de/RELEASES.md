# Release-Prozess

> 🌐 Sprache: [🇬🇧 English](../RELEASES.md) | **🇩🇪 Deutsch**

Dieses Dokument definiert den Release-Prozess für die eigenständige EasySteel-Firmware. Es stellt sicher, dass Quellcode, Firmware-Binärdateien und Prüfsummen nachvollziehbar und reproduzierbar sind.

## Versionierung

Firmware-Releases verwenden Semantic Versioning mit vorangestelltem `v` bei Git-Tags, zum Beispiel `v0.1.0`.

- Patch-Releases korrigieren abwärtskompatible Fehler.
- Minor-Releases fügen abwärtskompatible Funktionen hinzu.
- Major-Releases können inkompatible Konfigurations-, Protokoll- oder Hardwareänderungen enthalten.
- Das Protokoll v1 bleibt für jedes Release kompatibel, das `protocolVersion: 1` deklariert. Eine inkompatible Protokolländerung erfordert eine neue `protocolVersion`.

## Release-Checkliste

1. Sicherstellen, dass der Arbeitsbaum sauber ist und der Release-Commit geprüft wurde.
2. Vollständige Verifikations-Suite ausführen:

   ```powershell
   pio test -e native
   pio run -e esp32dev
   pio check -e esp32dev --skip-packages
   ```

3. Die Binärdatei `.pio/build/esp32dev/firmware.bin` auf das Referenzgerät flashen und Boot, Display, Tastereingaben, Treffererfassung, Ergebnisbestätigung, WLAN-Provisioning und OTA-Authentifizierung testen.
4. Die Binärdatei als versioniertes Release-Asset kopieren und die SHA-256-Prüfsumme erstellen:

   ```powershell
   Copy-Item .pio/build/esp32dev/firmware.bin EasySteel-firmware-vX.Y.Z.bin
   Get-FileHash EasySteel-firmware-vX.Y.Z.bin -Algorithm SHA256 |
     Format-List Algorithm,Hash,Path
   ```

5. Den signierten oder annotierten Git-Tag `vX.Y.Z` erst nach erfolgreicher Prüfung erstellen und pushen.
6. Ein GitHub-Release veröffentlichen, das Folgendes enthält:

   - den passenden Quellcode-Tag;
   - `EasySteel-firmware-vX.Y.Z.bin`;
   - eine `SHA256SUMS.txt`-Datei mit der Prüfsumme;
   - [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) und alle anwendbaren Drittanbieter-Lizenztexte;
   - prägnante Release Notes einschließlich bekannter Einschränkungen und Hardware-Kompatibilität.

7. Das Release als Entwurf (Draft) behalten, bis Lizenzhinweise, Drittanbieter-Audit, Security-Reporting-Kanal und Hardware-Dokumentation vollständig bereit sind.

## Release-Artefakte

Erzeugte Binärdateien und Prüfsummen sind Release-Artefakte. Sie dürfen nicht in das Git-Repository eingecheckt werden. Die exakten PlatformIO- und Abhängigkeitsversionen sind in [platformio.ini](../../platformio.ini) festgeschrieben.

Die Release Notes müssen auf [LICENSES.md](../../LICENSES.md) verweisen, Drittanbieter-Hinweise beibehalten und angeben, ob das Release ein fertigungsreifes Hardware-Designpaket enthält. Die Nutzung des Namens und Logos von EasySteel wird separat durch [TRADEMARKS.md](../../TRADEMARKS.md) geregelt.
