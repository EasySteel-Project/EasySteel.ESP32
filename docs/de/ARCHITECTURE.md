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

## Mobiles Einzeltraining

Ein Handy darf nur im Einzeltraining und nach einer ausdrücklichen physischen Freigabe über den eigenen Eintrag **Handy-Training** im Hauptmenü des ESP32 als nachrangige Fernbedienung arbeiten. Der ESP32 zeigt dafür 60 Sekunden lang eine vierstellige PIN an. Nach erfolgreicher Kopplung erhält genau ein Handy ein flüchtiges Session-Token. Fünf falsche PIN-Eingaben schließen das Pairing-Fenster.

Das gekoppelte Handy darf in `STANDBY` zwischen Wettkampf, Alle Ziele und Drill wechseln, einen Lauf starten, einen Countdown oder laufenden Durchgang stoppen sowie einen Trainingslauf zurücksetzen oder verwerfen. Die Anforderungen gelangen in einen begrenzten Command-Slot und werden von der ESP32-Zustandsmaschine geprüft; der Webserver schreibt niemals direkt Rundenstatus, Zeiten, Treffer oder Wertungsdaten.

Physische Bedienelemente haben immer Vorrang vor einer wartenden Handy-Anforderung. Eine Desktop-Verbindung widerruft die Handy-Session sofort und sperrt neues Pairing. Nach Trennung des Desktops wird die Handy-Steuerung nicht automatisch wiederhergestellt; sie muss erneut physisch freigegeben werden.

Abgeschlossene Einzeltrainingsläufe werden mit stabilen Ergebnis-IDs zum genau einmaligen Kopieren in den LocalStorage dieses Handys bereitgestellt. Diese Einträge sind nicht autoritative Trainingshistorie und gelangen niemals in die offizielle Ergebnis-Outbox.

## Ausfallverhalten

Der Timer muss bei Ausfall des WLANs, des gekoppelten Handys oder der Desktop-Anwendung uneingeschränkt einsatzfähig bleiben. Netzwerkaktivitäten dürfen die Zeitmessung zu keinem Zeitpunkt blockieren. Bestätigte offizielle Ergebnisse werden im RAM-Puffer für eine spätere Zustellung vorgehalten, der Zustellstatus darf jedoch niemals die gemessene Zeit oder den Hardware-Zustand verändern. Pairing und Handy-Steuerung enden sicher bei Timeout, Neustart, physischer Deaktivierung oder Desktop-Verbindung.

## Protokollkompatibilität

Das Protokoll v1 verwendet Zeilen-terminiertes UTF-8-JSON. Empfänger müssen unbekannte Eigenschaften ignorieren. Verhaltenserweiterungen, die diese Autoritätsregeln wahren, können abwärtskompatibel ergänzt werden; jede inkompatible Nachrichten- oder Semantikänderung erfordert eine neue Protokollversion (`protocolVersion`).
