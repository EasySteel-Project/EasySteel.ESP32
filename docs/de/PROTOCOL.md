# EasySteel TCP-Protokoll v1

> 🌐 Sprache: [🇬🇧 English](../PROTOCOL.md) | **🇩🇪 Deutsch**

Dieses Dokument ist der Integrationsvertrag zwischen der ESP32-Firmware und einer anzeigenden Desktop-Anwendung. Die Hardware bleibt für den vollständigen Rundenablauf führend.

## 1. Transport

- Der ESP32 arbeitet als TCP-Client und verbindet sich mit der konfigurierten Server-IP.
- Standard-Port: `53421`.
- Kodierung: UTF-8-kompatibles JSON ohne Binärdaten.
- Framing: genau ein JSON-Objekt pro Zeile, abgeschlossen mit `\n`; `\r\n` wird beim Empfang ebenfalls akzeptiert.
- Maximale ausgehende Nachrichtengröße: 384 Byte einschließlich Zeilenende und Nullterminierung im lokalen Puffer.
- Die JSON-Nutzlast einer eingehenden Zeile darf höchstens 191 Byte vor `\n` enthalten. Längere Zeilen werden verworfen.
- Empfänger müssen unbekannte JSON-Eigenschaften ignorieren.

## 2. Autoritätsmodell

Die Desktop-Anwendung darf Ereignisse anzeigen, bestätigte Ergebnisse speichern und deren Empfang quittieren. Sie darf keine Runde starten, stoppen, zurücksetzen oder korrigieren.

Nicht Bestandteil von v1 sind insbesondere Befehle wie:

```text
arm, disarm, start, stop, reset, set_time, set_hit, set_penalty
```

Capability und ACK steuern ausschließlich die Übertragung eines bereits auf der Hardware bestätigten Ergebnisses.

## 3. Gemeinsame Felder ausgehender Ereignisse

| Feld | Typ | Vorkommen | Bedeutung |
|---|---|---|---|
| `protocolVersion` | Integer | immer | Aktuell `1` |
| `deviceId` | String | immer | Stabile, MAC-basierte Gerätekennung |
| `deviceName` | String | immer | Konfigurierbarer Anzeigename |
| `event` | String | immer | Ereignistyp |
| `time` | Integer | immer | Millisekunden; bei Ereignissen ohne Zeitwert `0` |
| `targetCount` | Integer | immer | Aktuell konfigurierte Anzahl einschließlich Stop Plate |
| `target` | Integer | nur `hit` | Sensorindex `0` bis `4`; Index `0` ist die Stop Plate |
| `missedTargets` | Integer | immer bei `end` | Summe der beim bestätigten Ergebnis angewendeten Miss-/Strafpunkte |
| `runId` | String | nur `end` | Eindeutige Ergebniskennung innerhalb einer Boot-Sitzung |

`time` enthält bei `hit` die Splitzeit seit dem Startsignal und bei `end` die bestätigte Ergebniszeit einschließlich Strafen. Zeitwerte sind ganzzahlige Millisekunden.

## 4. Ereignisse vom ESP32

| Ereignis | Zeitpunkt | Zustellklasse |
|---|---|---|
| `online` | Nach erfolgreicher TCP-Verbindung | Live, Best Effort |
| `start_pressed` | Physischer Start-Taster beginnt den Vorlauf | Live, Best Effort |
| `start` | Startsignal nach Zufallsdelay | Live, Best Effort |
| `hit` | Verarbeiteter Sensortreffer | Live, Best Effort |
| `end` | Physisch bestätigtes Endergebnis | Ergebnis-Outbox |
| `reset` | Aktive oder fertige Runde wurde verworfen | Live, Best Effort |
| `settings_changed` | Geräteeinstellung wurde gespeichert | Live, Best Effort |
| `ping` | Heartbeat im Abstand von 10 Sekunden | Live, Best Effort |

### Beispiele

Verbindung:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"online","time":0,"targetCount":5}
```

Startsignal:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"start","time":0,"targetCount":5}
```

Treffer auf Sensorindex 2 nach 1.523 ms:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"hit","target":2,"time":1523,"targetCount":5}
```

Bestätigtes Ergebnis:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"end","time":4250,"targetCount":5,"runId":"91af20c3-7","missedTargets":0}
```

## 5. Nachrichten vom Server

### Capability-Aushandlung

Ein ACK-fähiger Server sollte innerhalb einer Sekunde nach Verbindungsaufbau senden:

```json
{"event":"capabilities","resultAck":true}
```

Mit `false` oder ohne rechtzeitige Capability arbeitet die Firmware im Legacy-Modus. Weitere unbekannte Felder werden ignoriert.

### Ergebnisbestätigung

Nach vollständigem Empfang und dauerhafter Verarbeitung eines `end`-Ereignisses bestätigt der Server exakt dessen `runId`:

```json
{"ack":"91af20c3-7"}
```

Ein ACK für eine unbekannte, noch nicht vollständig gesendete oder nicht älteste Ergebniskennung wird ignoriert.

## 6. Zustellgarantien

### Live-Ereignisse

- Statische FIFO mit 16 Nachrichten.
- Nur bei bestehender TCP-Verbindung erzeugt.
- Bei voller Queue verworfen.
- Bei Verbindungsabbruch nicht erneut gesendet.
- Dienen der Anzeige und dürfen nicht als alleinige Quelle eines Ergebnisses verwendet werden.

### Ergebnisse

- `end` wird erst nach physischer Bestätigung erzeugt.
- Statische RAM-Outbox mit 20 Ergebnissen, auch wenn aktuell keine TCP-Verbindung besteht.
- Nur das älteste Ergebnis wird gleichzeitig übertragen.
- ACK-Modus: Ergebnis bleibt bis zum passenden ACK erhalten und wird nach Reconnect erneut gesendet.
- Legacy-Modus: Ergebnis wird nach vollständig abgeschlossenem Socket-Schreibvorgang entfernt.
- Bei voller Outbox ersetzt ein neues Ergebnis den ältesten Eintrag.
- Neustart oder Stromausfall löscht die gesamte Outbox.

Die Desktop-Anwendung muss `runId` zur idempotenten Speicherung verwenden, weil Wiederholungen im ACK-Modus ausdrücklich möglich sind.

## 7. Kompatibilitätsregeln

- Erweiterungen innerhalb von v1 dürfen nur optionale Felder oder neue, ignorierbare Ereignisse hinzufügen.
- Bestehende Feldbedeutungen und Einheiten dürfen innerhalb von v1 nicht geändert werden.
- Verbraucher ignorieren unbekannte Felder und Ereignisse.
- Eine inkompatible Änderung erfordert eine neue `protocolVersion`.
- `runId` ist eine Transport- und Idempotenzkennung, keine Freigabe für einen Hardware-Zustandswechsel.
