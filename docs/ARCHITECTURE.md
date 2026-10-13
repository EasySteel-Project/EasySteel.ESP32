# System Authority

EasySteel is hardware-first. The ESP32 timer is the authoritative source for every fact that can affect a round or its result.

## Responsibilities

The ESP32 owns:

- round state and all state transitions;
- the random start delay and start signal;
- hit capture, chronological ordering, and timestamps;
- timeout and penalty calculation;
- range-officer confirmation, discard, and reset;
- the final result and its unique run ID.

The desktop application may:

- display live hardware events and connection status;
- persist confirmed results;
- manage shooters, stages, matches, and tournaments;
- provide bounded text context for the standby display;
- acknowledge receipt of final results.

The desktop application must not start, arm, stop, reset, correct, or otherwise override a round. An acknowledgement confirms transport receipt only; it is not permission for a hardware state transition.

## Mobile Solo Training

A phone may act as a low-priority remote control only for Solo Training after explicit physical release through the dedicated **Phone Training** entry in the ESP32 main menu. The ESP32 displays a four-digit PIN for 60 seconds. After successful pairing, exactly one phone receives a volatile session token. Five invalid PIN attempts close the pairing window.

The paired phone may select Competition, All Targets, or Drill while in `STANDBY`, start a run, stop a countdown or running round, and reset or discard a training run. Requests enter a bounded command slot and are validated by the ESP32 state machine; the web server never writes round state, timing, hits, or scoring data directly.

Physical controls always have priority over a queued phone request. A desktop connection immediately revokes the phone session and blocks new pairing. Desktop disconnection does not restore mobile control; a new physical release is required.

Completed Solo Training runs are exposed with stable result IDs for one history entry per run in that phone's LocalStorage. Physical penalty corrections update the same entry while preserving its timestamp and stage. These records are non-authoritative training history and never enter the official result outbox.

## Asynchronous HTTP ownership

`ESPAsyncWebServer` and `AsyncTCP` perform socket I/O on the AsyncTCP task. The loop task remains the sole owner of pairing and round mutations. Small HTTP requests enter a bounded mailbox; detached response objects carry the result back without retaining a network request pointer in the loop. Disconnected requests are cancelled before processing where possible; an action already accepted for execution can still complete after a disconnect. Clients must not automatically replay actions after a timeout.

The live state endpoint reads a mutex-protected snapshot, copied without heap allocation by the loop after round processing. The token is internal to the snapshot and is never included in status JSON. Polling records a bounded session-activity notification for the loop. Page responses own their replacement values and stream the flash template in bounded pieces; literal CSS percent signs are preserved.

HTTP bodies outside the firmware upload are limited to 512 bytes. At most four deferred requests/responses are retained, including responses waiting for slow clients. Queued requests expire after one second. Pending remote commands are also bound to their originating countdown and expire after one second; a Stop requested during DELAY may still stop the same run after it becomes RUNNING.

Browser OTA requires an authenticated `/update/prepare` reservation while in STANDBY. Its upload lease admits one upload; the main loop excludes physical and remote starts until success, failure, or 15 seconds without upload activity. Flash writes are serialized with cleanup. The browser path keeps the loop watchdog registered; Arduino OTA's task-local watchdog changes stay on the loop task. Successful browser updates schedule a restart; failed uploads release maintenance mode. Hardware verification of slow uploads, disconnects, and concurrent Arduino/browser OTA remains required.

RangeControl server addresses must be numeric IPv4 addresses. Reconnects use a non-blocking socket and zero-wait completion polling instead of synchronous DNS or connection waits. Attempts expire after 150 ms across loop iterations and are cancelled when leaving STANDBY or losing Wi-Fi. The loop refreshes its timestamp after network work before advancing the round controller.

## Failure behavior

The timer must remain usable when Wi-Fi, the paired phone, or the desktop application is unavailable. Network work must not block timing. Confirmed official results may be retained for later delivery, but delivery status must never alter the recorded time or round state. Mobile pairing and control expire safely on timeout, reboot, physical deactivation, or desktop connection.

## Protocol compatibility

Protocol v1 uses newline-terminated UTF-8 JSON. Consumers must ignore unknown properties. Behavioral extensions that preserve these authority rules may be added compatibly; any incompatible message or semantic change requires a new protocol version.
