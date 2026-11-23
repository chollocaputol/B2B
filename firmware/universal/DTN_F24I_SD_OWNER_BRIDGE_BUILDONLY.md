# F24I — independent-witness SD-owner bridge (BUILD-ONLY)

**Required local parent:** `c26cafd88afecf87df8d3c6d1e5bc756df0547c2`, clean `feature/firmware-multihop`.
**Operational scope:** Only SOS journal candidates are in scope for DTN. Normal peer LoRa packets and continuous research/weather logs are NOT uploaded by this work.

## Implemented

- `dtn_f24i_sd_owner_bridge.h` defines a typed `Witness` and an injected `GuardedPort<Port,Observer>`. It composes with the accepted F24H `Controller` using the same Port API.
- An initially unbound bridge can be armed once **only** when a live independent witness matches the F24H port snapshot: exact uploader/vessel/session, nonzero media/mount/source/lease/close generations, exclusive main-loop ownership, all-writes interception, no concurrent export/history/SOS/radio, and explicit (external, presently unavailable) physical authorization. A failed bind permanently revokes that bridge.
- Every requested quiesce, urgent/research drain, flush/close, SOS final-byte inspection and source protection is checked before and after its injected Port call. Only an append may advance the source generation, exactly once per completed record. Ownership changes, loss of permission, extra queued records, unexpected file mutation and faulted operations latch denial. SOS queue takes precedence over research queue.
- The bridge never creates a journal closure proof, digest, F24G seal, F24F `independentlyApproved` bit or cloud ACK. Successful host tests demonstrate only an interface contract using fabricated independent witness/Port values, NOT authenticity or SD durability.
- Build-only firmware uses **only** `urgentQueue.count` and `telemetryQueue.count` to populate `NoPhysicalPort`, and calls its default-denied bridge. This cannot mount/read/write SD, drain queues, retire a session or enable LoRa/HTTP/upload behavior. Existing logger runtime paths stay unchanged outside this preview.
- New profile: `tbeam-supreme-f24i-owner-bridge-deny-build-only`, derived from F24H and OFF by default. **DO NOT FLASH:** the firmware inherits normal operational radio transmissions if flashed; it is not globally RF-silent.

## Remaining hard hold

There is NO concrete real card owner/Port, verified SD-card identity, genuine sole-writer lock, unavoidable interception of existing `appendRecord()`/export/history paths, independent source-epoch witness, durable flush/close proof, power-loss recovery, physical write exclusion, final F24D full-file hash, F24G sidecar commit, or F24F replay authorization. No original V002 evidence/SD card was accessed. Those requirements need a separate reviewed implementation and explicit consent before any physical test on a **disposable sacrificial card**, never the preserved original V002 card.

In particular, the passing fake witness uses synthetic facts; no real firmware code can set the required authorization fields. Existing F06 SOS/telemetry/diagnostic journals remain historical evidence, not a cloud delivery queue.

## Acceptance

1. Guard exact F24H Git parent and clean branch. Verify header hashes and exact anchors.
2. Compile/run Windows native F24B, F24F, F24G, F24H regressions and F24I owner bridge tests against the operator's *actual local committed headers*.
3. Copy exactly five uncommitted files (three new, two modified). No Git add, commit or push yet.
4. PlatformIO ESP32-S3 **compile only** for the derived build-only environment; inspect the Git diff.
5. Separately review and commit locally after passing results; tracker acceptance afterward.

No physical SD, flash, Wi-Fi, HTTPS, production Supabase, relay permissions, credentials or Github remote changes are authorized by F24I.
