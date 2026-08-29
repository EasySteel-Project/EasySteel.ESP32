# EasySteel TCP Protocol v1

This document is the binding integration contract between EasySteel ESP32 firmware and RangeControl. Protocol v1 has no compatibility obligation to earlier development snapshots. The ESP32 remains authoritative for the complete round lifecycle and every value that affects a score.

## 1. Transport and framing

- The ESP32 is a TCP client. RangeControl is the TCP server.
- Default TCP port: `53421`.
- Each frame contains exactly one UTF-8 encoded JSON object followed by `\n`.
- Receivers also accept `\r\n`; the optional `\r` is delimiter data, not JSON payload.
- The JSON payload may contain at most **382 UTF-8 bytes** in either direction.
- A frame may therefore contain at most **383 bytes**, including `\n` but excluding an optional `\r`.
- Empty lines are ignored.
- Invalid UTF-8, invalid JSON, oversized frames, duplicate JSON properties, and messages that violate this contract are discarded as a complete frame.
- After an oversized frame begins, all bytes are discarded through the next `\n`.
- JSON property order is not significant.
- Unknown properties are ignored.
- Top-level JSON property names are unescaped 1–31 byte identifiers using the identifier character set below; duplicate top-level property names are invalid.

The terminating null byte used by C/C++ buffers is never transmitted and is not part of the protocol limit.

## 2. Security boundary

Protocol v1 does not provide encryption, peer authentication, or cryptographic message integrity. It is intended for a trusted and isolated range LAN.

- `deviceId` identifies a configured device but is not an authentication credential.
- RangeControl should maintain an allow-list that maps expected `deviceId` values to lanes.
- Untrusted clients must never be allowed to submit official results.
- Network-provided match context is display and correlation metadata only. It cannot change timing, hits, penalties, confirmation, reset, or any other round state.
- Deployments requiring an untrusted network must add a secure tunnel or a future authenticated protocol version.

## 3. Common envelope

Every message in both directions contains:

| Field | Type | Rule |
|---|---|---|
| `protocolVersion` | Integer | Required and exactly `1` |
| `event` | String | Required event discriminator; 1–31 ASCII bytes |

Event and identifier strings use only `A-Z`, `a-z`, `0-9`, `_`, `-`, `.`, and `:` unless a field explicitly permits display text.

Messages with a missing or unsupported `protocolVersion` are discarded. Within a connection, unknown v1 events are ignored by the ESP32 and may be surfaced as unknown events by RangeControl.

## 4. Device-to-server events

Every ESP32 event additionally contains:

| Field | Type | Rule |
|---|---|---|
| `deviceId` | String | Required stable device identifier; 1–31 identifier bytes |
| `deviceName` | String | Required printable UTF-8 display name; 1–31 bytes; no control characters |
| `timeMs` | Integer | Required, `0`–`30000`; event-specific meaning below |
| `targetCount` | Integer | Required, `1`–`5`, including the Stop plate |

### Event catalogue

| Event | Additional fields | `timeMs` | Delivery class |
|---|---|---|---|
| `online` | None | `0` | Live, best effort |
| `start_pressed` | None | `0` | Live, best effort |
| `start` | None | `0` | Live, best effort |
| `hit` | `target` | Split time since the start signal | Live, best effort |
| `end` | Result fields below | Final scored time | Reliable result outbox |
| `reset` | None | `0` | Live, best effort |
| `settings_changed` | None | `0` | Live, best effort |
| `ping` | None | `0` | Live, best effort, every 10 seconds |

For `hit`, `target` is required and must be in the range `0 <= target < targetCount`. Target `0` is the Stop plate.

### Confirmed result event

An `end` event is created only after physical Range Officer confirmation. It contains:

| Field | Type | Rule |
|---|---|---|
| `runId` | String | Required; 1–23 identifier bytes; unique within the device boot session |
| `rawTimeMs` | Integer | Required raw stopped time before penalties |
| `missCount` | Integer | Required automatically calculated misses, `0`–`targetCount - 1` |
| `penaltyCount` | Integer | Required total confirmed 3-second penalty units, `missCount`–`10` |
| `penaltyTimeMs` | Integer | Required penalty time actually applied after the 30-second cap |
| `contextId` | String | Optional unchanged match context; 1–47 identifier bytes |

The following invariants are binding:

```text
timeMs = min(rawTimeMs + penaltyCount * 3000, 30000)
penaltyTimeMs = timeMs - rawTimeMs
0 <= missCount <= penaltyCount <= 10
```

`missCount` records the automatically calculated minimum. Additional confirmed penalty units are represented by `penaltyCount - missCount`.

Example:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"end","timeMs":7250,"targetCount":5,"runId":"91af20c3-7","rawTimeMs":1250,"missCount":1,"penaltyCount":2,"penaltyTimeMs":6000,"contextId":"match-17:stage-3"}
```

The durable idempotency key used by RangeControl is the pair `(deviceId, runId)`. `runId` alone is sufficient only inside the device's current TCP connection.

## 5. Server-to-device messages

### Capability negotiation

RangeControl sends the following message immediately after accepting a connection:

```json
{"protocolVersion":1,"event":"capabilities","resultAck":true}
```

`resultAck` is required and must be exactly `true`. The first valid `capabilities` message must arrive within 1.0 second. Missing, false, late, or repeated capability negotiation closes the connection and preserves all queued results for a later reconnect.

### Result acknowledgement

RangeControl sends an ACK only after an `end` result has been durably stored or recognized as an identical duplicate:

```json
{"protocolVersion":1,"event":"ack","runId":"91af20c3-7"}
```

`runId` is required and must exactly match the oldest retained result. Unknown or out-of-order ACKs are ignored.

### Display-only match context

RangeControl may set the context shown in Standby:

```json
{"protocolVersion":1,"event":"match_info","contextId":"match-17:stage-3","stage":"Roundabout"}
```

Rules:

- `contextId` is an opaque identifier of 1–47 identifier bytes.
- `stage` is printable ASCII display text of 1–31 bytes without control characters.
- The ESP32 accepts `match_info` only while the round controller is in Standby.
- Context is locked throughout `DELAY`, `RUNNING`, and `FINISHED`.
- The accepted `contextId` is echoed unchanged in the confirmed `end` result.
- Context never changes timing, scoring, hits, penalties, or round state.
- To clear context, send both `contextId` and `stage` as empty strings while the device is in Standby.

The following remote round-control events are forbidden in v1 and must be ignored:

```text
arm, disarm, start, stop, reset, set_time, set_hit, set_penalty
```

## 6. Delivery guarantees

### Live events

- A static FIFO contains 16 frames.
- Live events are queued only while TCP is connected.
- Frames are sent in FIFO order with non-blocking socket writes.
- A full queue drops the new live event.
- Disconnect clears queued live events; they are never retransmitted.
- Live events are observational and must not be the sole official result record.

### Confirmed results

- A static RAM outbox retains 20 confirmed results, including all result and context fields.
- Results are transmitted oldest first, one at a time.
- Confirmed results provide **at-least-once delivery**.
- After a complete socket write, the firmware waits 3 seconds for the matching ACK.
- An ACK timeout schedules the same result for retransmission without requiring reconnect.
- Reconnect also schedules every retained result for retransmission.
- RangeControl must persist idempotently by `(deviceId, runId)` and ACK identical duplicates again.
- A duplicate key with different result data is a protocol-integrity error and must not overwrite the stored result automatically.
- A completed socket write never removes a result. Only the matching canonical ACK removes it from the outbox.
- If the outbox is full, a newly confirmed result replaces the oldest retained entry and the firmware emits a local warning.
- Reboot or power loss clears the RAM outbox.

## 7. Connection behavior

- The firmware attempts new TCP connections only while the round controller is idle, preventing connection setup from delaying active timing.
- An established connection may continue non-blocking receive and send work during a round.
- TCP preserves frame order, but best-effort live events can be absent because they were never queued or were discarded on disconnect.
- `online` is emitted after each successful connection.
- `online` must be the first valid device frame and may occur only once per connection.
- `deviceId` is immutable for the lifetime of a TCP connection; a change is a protocol violation and RangeControl closes the connection.
- `ping` is emitted every 10 seconds while connected.

## 8. Validation and compatibility

Known events are accepted only when every required field has the correct JSON type, range, byte length, and event-specific invariant. Invalid frames do not partially update device or RangeControl state.

Within protocol v1:

- New optional properties may be added and must be ignored by older v1 consumers.
- New events may be added and must be ignored safely by consumers that do not know them.
- Existing required fields, meanings, units, limits, and authority rules cannot change.
- An incompatible change requires a new `protocolVersion`.

Protocol v1 deliberately provides no compatibility with pre-v1 development message shapes such as `{"ack":"..."}`, `time`, or `missedTargets`.
