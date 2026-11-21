# DTN firmware gate — delayed shore synchronization (not LoRa multi-hop)

**Status (Nov 2026): required in the manuscript; not yet implemented or accepted.**
This guide makes no changes to radio, Wi-Fi, SD, NVS, SOS, credentials or identities.

## Thesis source and intended roles

- Sections 3.5.1 and 4.8.7: near-real-time LoRa forwarding for SOS and
  controlled emergency mesh; three-node RF proof remains blocked by V001.
- Sections 3.5.2 and 4.8.7: routine weather/GPS telemetry and unresolved SOS
  records cached on microSD during offshore disruption, physically carried
  ashore and bulk-uploaded over Wi-Fi to the backend.
- Section 4.9.2 item 9: asynchronous shore upload with explicit acceptance
  before a record is marked synchronized. Server arrival is not source UTC.

**Do not equate RAM-only LoRa relay queuing with durable DTN storage.**

## Verified current repository boundary

- Firmware has per-boot F06 microSD journals and export functionality.
- Default SD mode is SOS_ARMED: routine recording is OFF until CONTINUOUS
  is chosen on the OLED. A mounted card alone is NOT proof of complete
  routine DTN capture. Historical journals must remain untouched.
- No running firmware nearshore Wi-Fi reattachment + authenticated REST
  batch uploader, accepted-record cursor or reboot-safe retry process has
  been demonstrated. DTN bulk synchronization remains a work item.
- backend/docs/phase6-ingestion-api-contract.md is explicitly **proposed**
  and not yet an implemented service; its identity/auth and partial
  acceptance semantics must be decided before real uploads.
- No existing GPS/PHT HOME display change should be misrepresented as a
  trustworthy UTC timestamp in stored records or LoRa packets.

## Implementation gates before physical DTN acceptance

1. Decide whether/how routine logging becomes automatic at sea without
   silently overriding user-selected SOS_ARMED and emergency priorities.
2. Read immutable complete journal records, detect partial writes and
   preserve every previous per-vessel/per-session directory.
3. Assign stable record and batch identities; distinguish authenticated
   uploader hardware from unauthenticated LoRa origin.
4. Add a separately durable, crash-consistent sync checkpoint. Wi-Fi
   presence and generic HTTP 200 alone are not record acceptance;
   check per-record accepted/duplicate/rejected results and handle
   partial batches without marking rejected records synchronized.
5. Require secure device credentials, TLS validation, shore Wi-Fi
   connection gating, timeouts and SOS/LoRa preemption.
6. Host tests: duplicate replay; delayed acknowledgment; HTTP timeout
   after backend commit; corrupt last SD line; missing SD; reboot mid
   checkpoint; full card; disconnected shore Wi-Fi; rejected items.
7. Integrate only after backend API and schema are reviewed, then test
   real shore upload and deduplication separately from LoRa multihop.

The manuscript's absolute data-survival language is a desired research
outcome, NOT a measured guarantee under SD failure or lost equipment.

Next development unit: pure offline sync-cursor contract + host tests,
then read-only SD adapter; no live HTTP/Wi-Fi change in this commit.
