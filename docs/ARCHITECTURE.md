# System Authority

> 🌐 Language: **English** | [🇩🇪 Deutsch](de/ARCHITECTURE.md)

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

## Failure behavior

The timer must remain usable when Wi-Fi or the desktop application is unavailable. Network work must not block timing. Confirmed results may be retained for later delivery, but delivery status must never alter the recorded time or round state.

## Protocol compatibility

Protocol v1 uses newline-terminated UTF-8 JSON. Consumers must ignore unknown properties. Behavioral extensions that preserve these authority rules may be added compatibly; any incompatible message or semantic change requires a new protocol version.
