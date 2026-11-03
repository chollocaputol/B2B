# Phase 6 Ingestion API Contract

**Status:** Proposed for review; not implemented.

This document separates **repository facts**, **requirements supplied by the project team**, and **proposed API behavior**. The task describes the intended thesis workflow, but `MANUSCRIPT.pdf` is not present in the inspected repository, so this contract is based on the requirements supplied with the task and the checked-in source files.

## 1. Endpoint and versioning

- **Team decision:** Keep the existing Vite + React + TypeScript application as the dashboard. Device ingestion and privileged database operations belong in a separate backend API service. Do not introduce Next.js or a Supabase Edge Function.
- **Proposed endpoint:** `POST /api/v1/ingest` over HTTPS.
- **Repository fact (rechecked 2026-10-31):** `frontend/` is the Vite React/TypeScript dashboard (`frontend/package.json`, `frontend/vite.config.ts`, `frontend/src/`). Its package has React and TypeScript, but no backend API service is present in the inspected repository. Keep the dashboard and backend as separate applications/services; their deployment arrangement is not selected here.
- The separate backend API is responsible for device authentication, request and payload validation, registered vessel/device identity checks, backend LoRa-source trust classification, idempotency and duplicate handling, telemetry ingestion, SOS action processing and authorization, and secure Supabase database operations. It derives uploader identity from the authenticated credential and does not trust a body-supplied identity or origin-auth status.
- The Vite dashboard communicates with the backend through authenticated API requests for protected data and permitted actions. Dashboard user authentication and authorization remain separate from device credentials. Configure the backend's allowed browser origins and credential/token handling as part of deployment; do not place device credentials or Supabase service-role credentials in browser code.
- The backend framework/runtime, hosting/deployment platform, and whether the backend is deployed separately from or alongside the static Vite dashboard remain open decisions. No framework, platform, or hosting arrangement is implied by this contract.
- Use both a versioned URL (`v1`) and an integer `schema_version` in the request envelope. Do not infer or negotiate a new LoRa packet version through the HTTP API.

## 2. Authentication and identity rules

### Device HTTP authentication

Every physical ESP32 must be registered before ingestion is enabled. A registry entry maps a server-issued device UUID to a vessel and, through that vessel, to its registered LoRa node ID. Each device receives its own high-entropy HTTP credential. The API checks that the credential and device are active on every request; revocation and expiry take effect server-side. Credential rotation may allow a short, controlled overlap.

The examples below use `Authorization: Device <redacted-device-credential>` to make clear that this is device authentication, not a Supabase user JWT. The credential is never included in JSON, records, URLs, logs, or a source file. Its exact header syntax is a proposed choice.

The server derives `uploading_device_id` from the authenticated credential. The request body must not establish or override that identity. HTTP device credentials authenticate the uploader; they do **not** authenticate the original source of a relayed LoRa packet.

### LoRa origin authentication

**Repository fact:** The current v1 LoRa packet has no MAC. Per-device LoRa message authentication is a final-system requirement, but is not implemented.

An authenticated HTTP uploader is not sufficient proof of the original LoRa source. A submitted `origin_auth`/proof object is untrusted input, and a client-supplied status is never authoritative. The backend sets source-authentication status only after evaluating the agreed source-authentication mechanism:

- `VERIFIED`: the backend successfully verified the agreed source-authentication mechanism.
- `UNVERIFIED`: source authenticity has not been established (including a missing proof while the mechanism is not yet implemented).
- `INVALID`: required verification failed or the record violates a required security rule.

Do not infer `VERIFIED` merely because the authenticated uploader's registered node ID matches the claimed origin. The precise evidence format and verification mechanism are not selected here. Do not infer an algorithm, key provisioning method, or device security capability from this contract. A relay's claim that it verified a packet is not a substitute for backend verification.

The team must decide whether `INVALID` messages are rejected outright or retained only in a separate, access-restricted security audit. They must not be inserted as ordinary accepted telemetry/SOS. Whether `UNVERIFIED` records may be retained during staged development, and whether production quarantines or rejects them, also requires approval; production security must not be weakened to accommodate unfinished firmware.

### Dashboard users

Device ingestion and dashboard authentication are separate trust domains. Monitoring users authenticate through the dashboard's user-authentication flow (proposed: Supabase Auth), then call the backend API with an authenticated user request; device credentials cannot log in as users. Only a server-verified administrator or rescue-operator role may confirm rescue. Do not use user-editable profile metadata as an authorization source. Rescue confirmation is not accepted as a device ingestion record.

## 3. Request envelope

An individual upload is a one-record batch. A batch contains records from one authenticated uploader and may contain telemetry and SOS device actions. The batch ID is stable when retrying the exact same request body.

Illustrative values only:

```http
POST /api/v1/ingest
Authorization: Device <redacted-device-credential>
Content-Type: application/json
```

```json
{
  "schema_version": 1,
  "batch_id": "00000000-0000-4000-8000-000000000001",
  "records": [
    {
      "kind": "telemetry",
      "record_id": "00000000-0000-4000-8000-000000000002",
      "origin_node_id": 161,
      "boot_id": "00000000-0000-4000-8000-000000000003",
      "sequence_number": 42,
      "device_uptime_s": 215,
      "event_time_utc": null,
      "gps_fix": true,
      "latitude": 14.5995,
      "longitude": 120.9842,
      "shore_distance_m": null,
      "temperature_c": 29.4,
      "humidity_percent": 78.2,
      "pressure_hpa": 1008.4,
      "pressure_trend_hpa": -0.8,
      "zambretti_index": 12,
      "weather_state": 1,
      "battery_percent": null,
      "radio_receipt": null,
      "origin_auth_proof": null
    }
  ]
}
```

`origin_node_id: 161` is the illustrative decimal form of current Board A node ID `0xA1`; Board B is `0xB1` (decimal 177). It is a LoRa node number, not a credential or device UUID.

## 4. Telemetry record fields

| Field | Proposed type | Meaning and current firmware status |
|---|---|---|
| `kind` | string | `telemetry` or `relayed_message`; distinguishes record handling. |
| `record_id` | UUID string | Stable ID assigned once and retained with the queued record. **Not present in current firmware.** |
| `origin_node_id` | unsigned integer | Original LoRa sender. Current packet field is `uint32_t originNodeId`. |
| `boot_id` | UUID string | Stable session ID for one source boot. **Not present in current firmware.** Generate/persist once per boot session; a new boot must get a new ID. |
| `sequence_number` | unsigned integer | Current 32-bit source sequence. It resets to zero at reboot. |
| `device_uptime_s` | unsigned integer | Current packet's `timestamp`, seconds from `millis()/1000`; not UTC. |
| `event_time_utc` | RFC 3339 UTC timestamp or null | Source event time only when a trustworthy clock/GPS UTC reading is available. **Not transmitted by the current packet.** |
| `gps_fix` | boolean | Current `FLAG_GPS_FIX`. |
| `latitude`, `longitude` | finite number or null | Current signed e7 integers converted to decimal degrees. If no fix, send null coordinates. |
| `shore_distance_m` | nonnegative integer or null | Current firmware transmits zero as a placeholder; send null until measured. |
| `temperature_c`, `humidity_percent` | finite number or null | Current weather fields are tenths and require `FLAG_TEMP_HUMIDITY_VALID`; convert by dividing by 10. |
| `pressure_hpa`, `pressure_trend_hpa` | finite number or null | Current weather fields are tenths and require `FLAG_PRESSURE_VALID`; convert by dividing by 10. |
| `zambretti_index` | small integer or null | Current byte field; omit/null when invalid. |
| `weather_state` | integer 0–3 | Existing codes: calibrating, normal, front, squall. Preserve numeric encoding. |
| `battery_percent` | integer 0–100 or null | Current `batteryLevel=100` is a placeholder; send null until a real measurement exists. |
| `radio_receipt` | object or null | Optional receiver observation, e.g. hop count and RSSI/SNR. These are receiver-side values, not source measurements. Current original packet has `hopCount`, but transmitters set it to zero. |
| `origin_auth_proof` | opaque object or null | Untrusted evidence to be interpreted only by the backend under the mechanism later agreed by the team. The current firmware has no MAC or proof field. Do not put a client-supplied status here. |

The backend assigns the source-authentication status. The business tables may store `VERIFIED` or `UNVERIFIED`; `INVALID` is a failed verification outcome, not a trusted client value. If retained for investigation, store it in a separate restricted security audit record under the approved policy.

The current Board A packet also has `sensorFusionState`; the existing telemetry table does not. V1 should omit it unless the team approves an additive storage field. Board B's weather defaults are not sensor readings and must not be represented as valid measurements.

## 5. Relayed records and identity

A relay preserves the original record identity and payload. It does not replace the source with its own node ID. Authentication of the HTTP uploader comes from the request credential; the server obtains that device UUID and its registered vessel/node mapping independently.

Illustrative relay record:

```json
{
  "kind": "relayed_message",
  "record_id": "00000000-0000-4000-8000-000000000002",
  "origin_node_id": 161,
  "boot_id": "00000000-0000-4000-8000-000000000003",
  "sequence_number": 42,
  "device_uptime_s": 215,
  "event_time_utc": null,
  "payload": { "gps_fix": true, "latitude": 14.5995, "longitude": 120.9842, "weather_state": 1 },
  "radio_receipt": { "hop_count": 1, "rssi_dbm": -92, "snr_db": 5.5 },
  "origin_auth_proof": null
}
```

All values are illustrative. `uploading_device_id`, receiver identity, and authentication status are not trusted from this JSON; the server derives the uploader/receiver association from registration and verifies source evidence itself. The current firmware does not forward the original record, preserve original authentication evidence, or report a relay chain. Supporting those fields requires a separately reviewed LoRa protocol change. Until backend verification succeeds, an unauthenticated relayed source is `UNVERIFIED`; invalid proof is `INVALID` and follows the reject-versus-restricted-audit decision above.

## 6. SOS record types and lifecycle

**Repository fact:** Current firmware sends `PACKET_SOS = 2` with the normal sequence namespace and a text marker. There is no stable SOS ID, acknowledgment packet, cancellation packet, or rescue confirmation. Radio transmit success is not proof of receipt.

The ingest API may accept these device-originated records:

- `sos_activation`: stable `event_id`, stable `action_id`, source node, boot ID, sequence, uptime, optional UTC event time, GPS validity/location, and optional origin MAC.
- `sos_acknowledgment`: stable `event_id` and `action_id`, acknowledging device identity (derived from authentication), acknowledgement uptime/time, and optional untrusted source-authentication proof. The backend, not the receiver's JSON, assigns the source-authentication status. This represents a receiver's explicit protocol acknowledgment, not cloud acceptance or rescue.
- `sos_cancellation`: the same `event_id`, a new stable `action_id`, sender identity, source boot/sequence, and optional event time/reason. Require the authenticated source device to match the original event source unless a separate delegation policy is approved.

Illustrative activation:

```json
{
  "kind": "sos_activation",
  "event_id": "00000000-0000-4000-8000-000000000010",
  "action_id": "00000000-0000-4000-8000-000000000011",
  "origin_node_id": 161,
  "boot_id": "00000000-0000-4000-8000-000000000003",
  "sequence_number": 43,
  "device_uptime_s": 220,
  "event_time_utc": null,
  "gps_fix": true,
  "latitude": 14.5995,
  "longitude": 120.9842,
  "origin_auth_proof": null
}
```

Illustrative cancellation uses the same `event_id`, a distinct `action_id`, and `kind: "sos_cancellation"`. A cancellation means the sender withdrew the alert; it is never rescue confirmation.

The application must distinguish:

1. **ACTIVE** — event activated.
2. **ACKNOWLEDGED** — a receiver explicitly acknowledged the event through the agreed protocol; the alert remains active. Whether acknowledgment is accepted for an `UNVERIFIED` origin remains an approval decision and must be visible in the action record.
3. **CANCELLED** — the sender cancelled the event under the approved policy.
4. **RESCUE_CONFIRMED** — a separately authenticated administrator/rescue operator confirmed rescue.

Rescue confirmation should be a separately authorized user action (for example, a future `POST /api/v1/sos/{event_id}/rescue-confirmation` route), not an ingest record that an ESP32 can submit. Define before implementation: whether a sender can cancel after acknowledgment; how cancellation/rescue races are serialized and which action wins; whether repeated acknowledgment by the same receiver is a duplicate or a new audit action; whether an acknowledgment requires a verified origin; how delayed/reordered actions are handled; and whether cancellation can ever be reversed.

## 7. Timestamp semantics

- `device_uptime_s`: monotonic-ish seconds from the current boot, mapped from the present packet timestamp. It restarts on reboot and is meaningful only with `boot_id`.
- `event_time_utc`: optional source time, populated only when the firmware has reliable UTC. Current firmware does not send UTC.
- `received_at`: set by the server/database at successful ingestion. It records cloud receipt, not when the device observed the event.
- For SOS, the existing `emergency_alerts.triggered_at` is `NOT NULL`. Recommended compatibility approach: add a nullable source `event_time_utc`, preserve all historical `triggered_at` values, and in a new additive migration relax `triggered_at` to nullable after checking database consumers. For new events, set `triggered_at` only when trustworthy source UTC exists; otherwise leave it null and set server `received_at` independently. Do not put upload time in `triggered_at` or present it as activation time. If a consumer requires non-null `triggered_at`, migrate that consumer to `received_at` for arrival time and `event_time_utc` for source event time before relaxing the constraint.

Offline records may arrive long after their event time. Do not reject them solely because they are old. Validate source UTC when present, but retain server receipt time independently.

## 8. Validation and error handling

Validate request size, schema version, batch size, UUID syntax, record kind, required fields, integer ranges, finite numeric values, and action transitions. In particular:

- Node ID and sequence number must be valid unsigned 32-bit values for the current protocol.
- `weather_state` must be 0–3.
- If `gps_fix` is true, latitude/longitude are required and must be within ±90/±180 degrees. If false, coordinates should be null.
- Battery must be null or 0–100; hop count, distance, and packet counts must not be negative where applicable.
- Sensor values are nullable when the firmware validity flag is absent.
- Do not accept client-supplied uploader identity, `received_at`, or trusted origin-authentication status. Treat `origin_auth_proof` as untrusted until the backend verifies it.

Proposed response behavior:

| Condition | HTTP behavior | Queue behavior |
|---|---|---|
| Valid request, every item processed | `200` with per-record results | Checkpoint accepted/duplicate items only. |
| Well-formed batch with item validation failures | `200` with accepted/duplicate/rejected results per item | Keep rejected items; record reason. |
| Malformed envelope or unsupported API version | `400` | No item acknowledged. |
| Missing/invalid credential | `401`; revoked/disabled device may be `403` | Retain all items. |
| Request/batch too large | `413` | Retain all items; split/retry. |
| Rate limited | `429` with retry guidance | Retain all items. |
| Transient server/database failure | `5xx` | Retain and retry the exact batch/IDs. |
| Same ID with changed content | Per-item `rejected` conflict (or `409` if the entire request conflicts) | Retain and quarantine for correction; never overwrite silently. |

Exact limits and error-code vocabulary remain to be approved.

## 9. Batch results, idempotency, and microSD acknowledgments

Illustrative response:

```json
{
  "schema_version": 1,
  "batch_id": "00000000-0000-4000-8000-000000000001",
  "uploading_device_id": "00000000-0000-4000-8000-000000000020",
  "results": [
    { "record_id": "00000000-0000-4000-8000-000000000002", "status": "accepted", "origin_auth_status": "UNVERIFIED" },
    { "record_id": "00000000-0000-4000-8000-000000000004", "status": "duplicate" },
    { "record_id": "00000000-0000-4000-8000-000000000005", "status": "rejected", "code": "invalid_weather_state" }
  ]
}
```

The response's uploader ID and `origin_auth_status` are server-derived; values are illustrative. Only backend verification of the agreed source-authentication mechanism may produce `VERIFIED`. `INVALID` means verification failed or a required security rule was violated; reject it or retain it only in the approved restricted security audit, never as ordinary accepted data. **`accepted` means the database transaction committed. `duplicate` means an identical record/action is already durably stored.** Neither means a LoRa peer acknowledged SOS or that rescue occurred. A duplicate with different content is a conflict, not a success.

The future microSD JSONL queue should persist `record_id`, `boot_id`, sequence, and payload at record creation. Keep queued records until a response identifies them as accepted or identical duplicates. For partial batches, durably write an acknowledgment/checkpoint journal before reclaiming queue storage. If a response is lost or power fails before checkpointing, retry the same IDs and exact content. Keep rejected records and their error codes for repair; do not let one rejected item prevent accepted items from being checkpointed.

## 10. Device registration and user authorization

Provision a device row linked to a vessel before issuing credentials. Device registration must supply or resolve the expected LoRa node ID; do not create registrations from arbitrary ingestion JSON. Enforce device active/revoked status on each request.

Dashboard personnel authenticate through user auth, not the device token flow. User authorization must be checked server-side for vessel visibility and rescue confirmation. Only administrators/rescue operators may confirm rescue; vessel users may activate/cancel their own SOS but cannot confirm rescue. The exact role source and vessel-scope policy require approval.

Never place Supabase service-role credentials in firmware, frontend/browser bundles, JSONL records, or API responses. Keep privileged credentials in the backend service's server-only runtime configuration. Do not log device tokens, MAC keys, or authorization headers.

## 11. Current firmware gaps requiring future work

These are not current capabilities:

- Stable telemetry record UUID and persistent boot/session UUID.
- Persistent SOS event ID and action IDs for activation/cancel.
- Explicit LoRa acknowledgment packet and sender-side acknowledgment state.
- Per-device LoRa MAC generation and verification.
- Original packet forwarding with source identity/MAC preserved and separate receiver/link metadata.
- Reliable UTC event-time capture/encoding.
- Real battery measurement and shore-distance measurement.
- microSD JSONL append-only queue and durable acceptance/checkpoint journal.
- Wi-Fi/HTTPS HTTP client, device credential rotation/revocation handling, and non-blocking synchronization.

Any LoRa packet changes must be versioned and coordinated across Board A and Board B. This document does not prescribe a packet layout change.

### Production requirements versus staged development

| Field/capability | Current availability | Staged development | New production records |
|---|---|---|---|
| Authenticated HTTP uploader | Absent | Test credential may be used in an isolated test environment. | Mandatory; server resolves an active registered device. |
| Stable telemetry `record_id` | Absent | May be omitted only in isolated schema/API tests that do not claim durable retry guarantees. | Mandatory for telemetry and retries. |
| Source `boot_id` | Absent | May be omitted only in isolated tests that do not test reboot-safe deduplication. | Mandatory with source sequence for telemetry and source SOS actions. |
| SOS `event_id` and `action_id` | Absent | May be omitted only in non-production contract mocks. | Mandatory for activation, acknowledgment, and cancellation actions. |
| Reliable UTC event time | Absent from packet | Optional; send null when unavailable. | Optional when genuinely unavailable; if supplied, validate and retain separately from uptime/receipt. |
| Source-authentication proof | Absent | Records may be retained as `UNVERIFIED` only if the team approves an isolated/staged policy. | Required to mark source `VERIFIED`; an unverified source must remain labeled and must follow the approved production quarantine/rejection policy. |
| Original packet forwarding and receiver metadata | Absent | May be tested with explicitly marked unverified fixtures. | Required for relayed records; preserve source identity and evidence. |
| Server receipt time | Not supplied by device | Always assigned by server. | Always assigned by server; never accepted from payload. |
| Battery/shore-distance values | Placeholder values only | Null unless measured. | Null unless measured; never treat current constants as observations. |

This staging allowance does not weaken production authentication, identity, or acknowledgment requirements.
