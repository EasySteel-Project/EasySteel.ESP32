# EasySteel TCP Protocol v1

> 🌐 Language: **English** | [🇩🇪 Deutsch](de/PROTOCOL.md)

This document is the binding integration contract between the ESP32 firmware and an external desktop display application. The hardware remains authoritative for the entire round lifecycle.

## 1. Transport

- The ESP32 acts as a TCP client connecting to the configured desktop server IP.
- Default port: `53421`.
- Encoding: UTF-8 compatible JSON without binary payloads.
- Framing: Exactly one JSON object per line, terminated with `\n`; `\r\n` is also accepted on receive.
- Maximum outgoing message size: 384 bytes including newline and null termination in the local buffer.
- Incoming JSON lines must not exceed 191 bytes before `\n`. Longer lines are discarded.
- Consumers must ignore unknown JSON properties.

## 2. Authority Model

The desktop application may display events, persist confirmed results, and acknowledge receipt. It must not start, stop, reset, or alter a round.

The following remote commands are explicitly excluded from protocol v1:

```text
arm, disarm, start, stop, reset, set_time, set_hit, set_penalty
```

Capabilities and acknowledgements govern the reliable transport of results confirmed physically on the hardware.

## 3. Common Outgoing Event Fields

| Field | Type | Presence | Description |
|---|---|---|---|
| `protocolVersion` | Integer | Always | Currently `1` |
| `deviceId` | String | Always | Stable, MAC-derived device identifier |
| `deviceName` | String | Always | Configured friendly device name |
| `event` | String | Always | Event name identifier |
| `time` | Integer | Always | Milliseconds; `0` for events without an associated timestamp |
| `targetCount` | Integer | Always | Currently configured target count including Stop Plate |
| `target` | Integer | Only `hit` | Sensor index `0` to `4`; index `0` is the Stop Plate |
| `missedTargets` | Integer | Always in `end` | Total penalty/miss count applied to the confirmed result |
| `runId` | String | Only `end` | Unique result identifier within a boot session |

For `hit` events, `time` contains the split time in milliseconds since the start beep. For `end` events, `time` contains the final scored time including penalty seconds.

## 4. Outgoing Events from ESP32

| Event | Trigger | Delivery Class |
|---|---|---|
| `online` | Emitted upon successful TCP connection | Live, Best Effort |
| `start_pressed` | Physical Start button initiates countdown delay | Live, Best Effort |
| `start` | Start buzzer signal after random delay | Live, Best Effort |
| `hit` | Processed sensor hit | Live, Best Effort |
| `end` | Physically confirmed final result | Result Outbox |
| `reset` | Active or finished round discarded | Live, Best Effort |
| `settings_changed` | Device configuration saved to NVS | Live, Best Effort |
| `ping` | Heartbeat emitted every 10 seconds | Live, Best Effort |

### JSON Examples

Connection online:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"online","time":0,"targetCount":5}
```

Start signal:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"start","time":0,"targetCount":5}
```

Target 2 hit at 1,523 ms:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"hit","target":2,"time":1523,"targetCount":5}
```

Confirmed round result:

```json
{"protocolVersion":1,"deviceId":"EasySteel_6134","deviceName":"Lane 1","event":"end","time":4250,"targetCount":5,"runId":"91af20c3-7","missedTargets":0}
```

## 5. Incoming Messages from Server

### Capability Negotiation

An ACK-capable server should transmit capability within 1.0 second of connection:

```json
{"event":"capabilities","resultAck":true}
```

With `false` or without timely capability negotiation, the firmware defaults to legacy delivery. Unknown properties are ignored.

### Result Acknowledgement

After successfully receiving and persisting an `end` event, the server acknowledges its exact `runId`:

```json
{"ack":"91af20c3-7"}
```

Acknowledgements for unknown, partially transmitted, or non-head result entries are safely ignored.

## 6. Delivery Guarantees

### Live Events

- Static FIFO queue with 16 message slots.
- Enqueued only when an active TCP connection exists.
- Dropped if the queue is full.
- Discarded upon disconnection; never retransmitted.
- Intended for live UI display; must not be the sole source of record for official match results.

### Final Results

- `end` is created strictly after physical Range Officer confirmation.
- Static volatile RAM outbox with 20 slots, retained even when offline.
- Only the oldest result is transmitted at any time.
- ACK mode: Result is retained until matching ACK arrives; retransmitted upon reconnection.
- Legacy mode: Result is removed once the non-blocking socket write completes.
- If the outbox overflows, the newest result overwrites the oldest entry and logs a warning.
- Reboot or power loss clears the volatile RAM outbox.

The desktop application must treat `runId` as an idempotency key because network retries in ACK mode may deliver duplicate results.

## 7. Compatibility Rules

- Minor extensions within v1 must use optional fields or ignorable events.
- Existing field semantics and units cannot be altered in v1.
- Consumers must ignore unknown properties and events.
- Incompatible changes require incrementing `protocolVersion`.
- `runId` is a transport and idempotency token, never permission for a hardware state transition.
