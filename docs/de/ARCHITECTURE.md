# System-Autorität & Integrationsgrenzen

> 🌐 Sprache: [🇬🇧 English](../ARCHITECTURE.md) | **🇩🇪 Deutsch**

EasySteel folgt dem Grundsatz **Hardware First**. Der ESP32-Timer ist die alleinige, autoritative Quelle für alle Vorgänge, die einen Rundenablauf oder dessen Ergebnis beeinflussen.

## Verantwortlichkeiten

Der ESP32 besitzt und steuert exklusiv:

- den Rundenstatus und alle Zustandsübergänge;
- das zufällige Startdelay und das akustische Startsignal;
- die Treffererfassung, chronologische Reihenfolge und Zeitstempel;
- Timeout-Überwachung und Strafzeiten-Berechnung;
- die Bestätigung, Verwerfung und den Reset durch den Range Officer (Sicherheitsbeauftragten);
- das finale Ergebnis und dessen eindeutige `runId`.

Die Desktop-Anwendung darf:

- Live-Hardware-Ereignisse und den Verbindungsstatus anzeigen;
- bestätigte Ergebnisse dauerhaft speichern;
- Schützen, Stages, Matches und Turniere verwalten;
- begrenzten Textkontext für das Standby-Display bereitstellen;
- den erfolgreichen Empfang von Endergebnissen quittieren (ACK).

Die Desktop-Anwendung darf **unter keinen Umständen** eine Runde starten, scharfschalten (`arm`), stoppen, zurücksetzen, korrigieren oder anderweitig übersteuern. Eine Quittierung (ACK) bestätigt ausschließlich den Transportempfang; sie ist niemals die Erlaubnis für einen Zustandswechsel der Hardware.

## Ausfallverhalten

Der Timer muss bei Ausfall des WLANs oder der Desktop-Anwendung uneingeschränkt einsatzfähig bleiben. Netzwerkaktivitäten dürfen die Zeitmessung zu keinem Zeitpunkt blockieren. Bestätigte Ergebnisse werden im RAM-Puffer für eine spätere Zustellung vorgehalten, der Zustellstatus darf jedoch niemals die gemessene Zeit oder den Hardware-Zustand verändern.

## Protokollkompatibilität

Das Protokoll v1 verwendet Zeilen-terminiertes UTF-8-JSON. Empfänger müssen unbekannte Eigenschaften ignorieren. Verhaltenserweiterungen, die diese Autoritätsregeln wahren, können abwärtskompatibel ergänzt werden; jede inkompatible Nachrichten- oder Semantikänderung erfordert eine neue Protokollversion (`protocolVersion`).
