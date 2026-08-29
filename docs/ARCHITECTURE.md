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

Completed Solo Training runs are exposed with stable result IDs for exactly-once copying to that phone's LocalStorage. These records are non-authoritative training history and never enter the official result outbox.

## Failure behavior

The timer must remain usable when Wi-Fi, the paired phone, or the desktop application is unavailable. Network work must not block timing. Confirmed official results may be retained for later delivery, but delivery status must never alter the recorded time or round state. Mobile pairing and control expire safely on timeout, reboot, physical deactivation, or desktop connection.

## Protocol compatibility

Protocol v1 uses newline-terminated UTF-8 JSON. Consumers must ignore unknown properties. Behavioral extensions that preserve these authority rules may be added compatibly; any incompatible message or semantic change requires a new protocol version.
