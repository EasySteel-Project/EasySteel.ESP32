# EasySteel TCP-Protokoll v1

> 🌐 Sprache: [🇬🇧 English](../PROTOCOL.md) | **🇩🇪 Deutsch**

Dieses Dokument ist der verbindliche Integrationsvertrag zwischen der EasySteel-ESP32-Firmware und RangeControl. Protokoll v1 muss nicht mit früheren Entwicklungsständen kompatibel sein. Der ESP32 bleibt für den vollständigen Rundenablauf und alle wertungsrelevanten Werte autoritativ.

## 1. Transport und Framing

- Der ESP32 ist TCP-Client. RangeControl ist TCP-Server.
- Standard-TCP-Port: `53421`.
- Jeder Frame enthält genau ein UTF-8-codiertes JSON-Objekt mit abschließendem `\n`.
- Empfänger akzeptieren außerdem `\r\n`; das optionale `\r` gehört zum Trennzeichen und nicht zur JSON-Nutzlast.
- Die JSON-Nutzlast darf in beiden Richtungen höchstens **382 UTF-8-Bytes** enthalten.
- Ein Frame darf damit einschließlich `\n`, aber ohne optionales `\r`, höchstens **383 Bytes** enthalten.
- Leere Zeilen werden ignoriert.
- Ungültiges UTF-8, ungültiges JSON, zu große Frames, doppelte JSON-Eigenschaften und vertragswidrige Nachrichten werden als vollständiger Frame verworfen.
- Nach Beginn eines zu großen Frames werden alle Bytes bis zum nächsten `\n` verworfen.
- Die Reihenfolge der JSON-Eigenschaften ist nicht relevant.
- Unbekannte Eigenschaften werden ignoriert.
- JSON-Eigenschaftsnamen auf oberster Ebene sind nicht maskierte Identifikatoren mit 1–31 Bytes aus dem unten definierten Zeichensatz; doppelte Eigenschaftsnamen auf oberster Ebene sind ungültig.

Das in C/C++-Puffern verwendete abschließende Nullbyte wird nicht übertragen und gehört nicht zur Protokollgrenze.

## 2. Sicherheitsgrenze

Protokoll v1 bietet keine Verschlüsselung, gegenseitige Authentifizierung oder kryptografische Nachrichtenintegrität. Es ist für ein vertrauenswürdiges und isoliertes Range-LAN vorgesehen.

- `deviceId` identifiziert ein konfiguriertes Gerät, ist aber kein Authentifizierungsnachweis.
- RangeControl sollte eine Allowlist führen, die erwartete `deviceId`-Werte den Lanes zuordnet.
- Nicht vertrauenswürdige Clients dürfen keine offiziellen Resultate einreichen.
- Vom Netzwerk bereitgestellter Match-Kontext dient nur Anzeige und Zuordnung. Er darf Timing, Treffer, Strafen, Bestätigung, Reset oder einen anderen Rundenzustand nicht verändern.
- Installationen in nicht vertrauenswürdigen Netzen benötigen einen sicheren Tunnel oder eine zukünftige authentifizierte Protokollversion.

## 3. Gemeinsamer Umschlag

Jede Nachricht enthält in beiden Richtungen:

| Feld | Typ | Regel |
|---|---|---|
| `protocolVersion` | Integer | Erforderlich und exakt `1` |
| `event` | String | Erforderlicher Ereignis-Diskriminator; 1–31 ASCII-Bytes |

Ereignis- und Identifikator-Strings verwenden ausschließlich `A-Z`, `a-z`, `0-9`, `_`, `-`, `.` und `:`, sofern ein Feld nicht ausdrücklich Anzeigetext erlaubt.

Nachrichten mit fehlender oder nicht unterstützter `protocolVersion` werden verworfen. Unbekannte v1-Ereignisse ignoriert der ESP32; RangeControl darf sie als unbekannte Ereignisse melden.

## 4. Ereignisse vom Gerät zum Server

Jedes ESP32-Ereignis enthält zusätzlich:

| Feld | Typ | Regel |
|---|---|---|
| `deviceId` | String | Erforderliche stabile Gerätekennung; 1–31 Identifikator-Bytes |
| `deviceName` | String | Erforderlicher druckbarer UTF-8-Anzeigename; 1–31 Bytes; keine Steuerzeichen |
| `timeMs` | Integer | Erforderlich, `0`–`30000`; ereignisspezifische Bedeutung siehe unten |
| `targetCount` | Integer | Erforderlich, `1`–`5`, einschließlich Stop plate |

### Ereigniskatalog

| Ereignis | Zusätzliche Felder | `timeMs` | Zustellklasse |
|---|---|---|---|
| `online` | Keine | `0` | Live, Best Effort |
| `start_pressed` | Keine | `0` | Live, Best Effort |
| `start` | Keine | `0` | Live, Best Effort |
| `hit` | `target` | Split seit dem Startsignal | Live, Best Effort |
| `end` | Resultatfelder unten | Finale Wertungszeit | Zuverlässige Resultat-Outbox |
| `reset` | Keine | `0` | Live, Best Effort |
| `settings_changed` | Keine | `0` | Live, Best Effort |
| `ping` | Keine | `0` | Live, Best Effort, alle 10 Sekunden |

Bei `hit` ist `target` erforderlich und muss `0 <= target < targetCount` erfüllen. Target `0` ist die Stop plate.

### Bestätigtes Resultat

Ein `end`-Ereignis entsteht erst nach physischer Bestätigung durch den Range Officer. Es enthält:

| Feld | Typ | Regel |
|---|---|---|
| `runId` | String | Erforderlich; 1–23 Identifikator-Bytes; innerhalb der Geräte-Boot-Sitzung eindeutig |
| `rawTimeMs` | Integer | Erforderliche gestoppte Rohzeit vor Strafen |
| `missCount` | Integer | Erforderliche automatisch berechnete Misses, `0`–`targetCount - 1` |
| `penaltyCount` | Integer | Erforderliche bestätigte Gesamtzahl von 3-Sekunden-Strafeinheiten, `missCount`–`10` |
| `penaltyTimeMs` | Integer | Erforderlicher nach dem 30-Sekunden-Limit tatsächlich angewendeter Strafanteil |
| `contextId` | String | Optionaler unveränderter Match-Kontext; 1–47 Identifikator-Bytes |

Folgende Invarianten sind verbindlich:

```text
timeMs = min(rawTimeMs + penaltyCount * 3000, 30000)
penaltyTimeMs = timeMs - rawTimeMs
0 <= missCount <= penaltyCount <= 10
```

`missCount` hält das automatisch berechnete Minimum fest. Zusätzliche bestätigte Strafeinheiten ergeben sich aus `penaltyCount - missCount`.

Beispiel:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"end","timeMs":7250,"targetCount":5,"runId":"91af20c3-7","rawTimeMs":1250,"missCount":1,"penaltyCount":2,"penaltyTimeMs":6000,"contextId":"match-17:stage-3"}
```

Der dauerhafte Idempotenzschlüssel in RangeControl ist das Paar `(deviceId, runId)`. Nur innerhalb der aktuellen TCP-Verbindung des Geräts genügt `runId` allein.

## 5. Nachrichten vom Server zum Gerät

### Capability-Aushandlung

RangeControl sendet unmittelbar nach Annahme einer Verbindung:

```json
{"protocolVersion":1,"event":"capabilities","resultAck":true}
```

`resultAck` ist erforderlich und muss exakt `true` sein. Die erste gültige `capabilities`-Nachricht muss innerhalb von 1,0 Sekunden eintreffen. Fehlende, falsche, verspätete oder wiederholte Capability-Aushandlung schließt die Verbindung und bewahrt alle vorgemerkten Resultate für einen späteren Reconnect auf.

### Resultatbestätigung

RangeControl sendet ein ACK erst, nachdem ein `end`-Resultat dauerhaft gespeichert oder als identisches Duplikat erkannt wurde:

```json
{"protocolVersion":1,"event":"ack","runId":"91af20c3-7"}
```

`runId` ist erforderlich und muss exakt zum ältesten vorgehaltenen Resultat passen. Unbekannte oder nicht der Reihenfolge entsprechende ACKs werden ignoriert.

### Reiner Anzeige-Match-Kontext

RangeControl darf den in Standby angezeigten Kontext setzen:

```json
{"protocolVersion":1,"event":"match_info","contextId":"match-17:stage-3","stage":"Roundabout"}
```

Regeln:

- `contextId` ist ein opaker Identifikator mit 1–47 Identifikator-Bytes.
- `stage` ist druckbarer ASCII-Anzeigetext mit 1–31 Bytes ohne Steuerzeichen.
- Der ESP32 akzeptiert `match_info` ausschließlich im Standby-Zustand des Round Controllers.
- Der Kontext bleibt während `DELAY`, `RUNNING` und `FINISHED` unveränderlich.
- Die akzeptierte `contextId` wird im bestätigten `end`-Resultat unverändert zurückgegeben.
- Kontext verändert niemals Timing, Wertung, Treffer, Strafen oder Rundenzustand.
- Zum Löschen werden `contextId` und `stage` im Standby als leere Strings gesendet.

Folgende entfernte Rundensteuerungsereignisse sind in v1 verboten und müssen ignoriert werden:

```text
arm, disarm, start, stop, reset, set_time, set_hit, set_penalty
```

## 6. Zustellgarantien

### Live-Ereignisse

- Eine statische FIFO enthält 16 Frames.
- Live-Ereignisse werden nur bei bestehender TCP-Verbindung eingereiht.
- Frames werden in FIFO-Reihenfolge mit nichtblockierenden Socket-Schreibvorgängen gesendet.
- Bei voller Queue wird das neue Live-Ereignis verworfen.
- Ein Disconnect löscht wartende Live-Ereignisse; sie werden nicht erneut übertragen.
- Live-Ereignisse dienen der Beobachtung und dürfen nicht die einzige offizielle Resultatquelle sein.

### Bestätigte Resultate

- Eine statische RAM-Outbox hält 20 bestätigte Resultate samt aller Resultat- und Kontextfelder.
- Resultate werden vom ältesten zum neuesten und einzeln übertragen.
- Bestätigte Resultate bieten **At-least-once-Zustellung**.
- Nach vollständigem Socket-Schreibvorgang wartet die Firmware 3 Sekunden auf das passende ACK.
- Ein ACK-Timeout plant dasselbe Resultat ohne erforderlichen Reconnect erneut ein.
- Ein Reconnect plant ebenfalls alle vorgehaltenen Resultate erneut ein.
- RangeControl muss anhand `(deviceId, runId)` idempotent speichern und identische Duplikate erneut bestätigen.
- Gleicher Schlüssel mit abweichenden Resultatdaten ist ein Protokollintegritätsfehler und darf das gespeicherte Resultat nicht automatisch überschreiben.
- Ein abgeschlossener Socket-Schreibvorgang entfernt niemals ein Resultat. Nur das passende kanonische ACK entfernt es aus der Outbox.
- Ist die Outbox voll, ersetzt ein neu bestätigtes Resultat den ältesten Eintrag und die Firmware gibt lokal eine Warnung aus.
- Neustart oder Stromausfall löscht die RAM-Outbox.

## 7. Verbindungsverhalten

- Die Firmware beginnt neue TCP-Verbindungen ausschließlich im Idle-Zustand des Round Controllers, damit Verbindungsaufbau aktives Timing nicht verzögert.
- Eine bestehende Verbindung darf während einer Runde nichtblockierende Empfangs- und Sendeaufgaben fortsetzen.
- TCP erhält die Frame-Reihenfolge; Best-Effort-Live-Ereignisse können dennoch fehlen, weil sie nicht eingereiht oder beim Disconnect verworfen wurden.
- Nach jeder erfolgreichen Verbindung wird `online` gesendet.
- `online` muss der erste gültige Geräte-Frame sein und darf pro Verbindung nur einmal auftreten.
- `deviceId` bleibt während der gesamten TCP-Verbindung unverändert; eine Änderung ist eine Protokollverletzung und RangeControl beendet die Verbindung.
- Während bestehender Verbindung wird alle 10 Sekunden `ping` gesendet.

## 8. Validierung und Kompatibilität

Bekannte Ereignisse werden nur akzeptiert, wenn jedes Pflichtfeld den korrekten JSON-Typ, Wertebereich, die Bytegrenze und ereignisspezifische Invarianten erfüllt. Ungültige Frames aktualisieren weder Geräte- noch RangeControl-Zustand teilweise.

Innerhalb von Protokoll v1 gilt:

- Neue optionale Eigenschaften dürfen ergänzt werden und müssen von älteren v1-Empfängern ignoriert werden.
- Neue Ereignisse dürfen ergänzt werden und müssen von unbekannten Empfängern sicher ignoriert werden.
- Bestehende Pflichtfelder, Bedeutungen, Einheiten, Grenzen und Autoritätsregeln dürfen sich nicht ändern.
- Eine inkompatible Änderung erfordert eine neue `protocolVersion`.

Protokoll v1 bietet ausdrücklich keine Kompatibilität zu Entwicklungsversionen wie `{"ack":"..."}`, `time` oder `missedTargets`.
