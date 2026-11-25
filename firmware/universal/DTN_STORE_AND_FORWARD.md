# DTN Store-and-Forward — SF7 consolidation

Source checkpoint [`c32177d`](https://github.com/chollocaputol/B2B/commit/c32177dd7605a4fff0e8d2174f30d440e83a301d); synchronization reference [`873371d`](https://github.com/chollocaputol/B2B/commit/873371d758bfd1ef4133a4c632d11c76c050776e). Integration base `217013fe170cc45761be240def177b8b0c03db74`. The original feature branch and its historical evidence remain intact. The consolidated implementation has been separately validated within the scope documented below; prior SF7 results alone were not treated as integration acceptance.

## Supported profiles and boundaries

- `tbeam-supreme-universal-study`: common offline image; existing enrolled identity; Wi-Fi upload disabled; shared-wiring sensor policy; SOS_ARMED at boot, CONTINUOUS selectable through DEV8; no new relay authority. SF7 smoke tested on V002/V003. Exact V002 flash transcript and reverse RX excerpt were not supplied; not formal PDR or field acceptance.
- `tbeam-supreme-study-sf-v003-sync-bench`: V003-only, temporary generated local configuration, Wi-Fi/SNTP/TLS and authenticated local receipts. SF6 bench passed; no fleet provisioning, production or field claim. Never include generated configuration or resulting credential-bearing binaries in Git.
- `tbeam-supreme-study-sf-v003-offline-bench`: retained isolated V003 historical storage bench.
- `tbeam-supreme-study-sf-build-only`: unconfigured compile target, no runtime storage grant. Not an upload target; ordinary inherited radio activity may exist if improperly flashed.

## Runtime dependencies and semantics

Use existing GNSS, independent one-minute micro/ten-minute macro, 30-second GPS sample, and original local/received SOS/CANCEL capture hooks. One eight-record RAM queue, one worker owns the study journal on native SPI3 SD while LoRa uses its existing controller. Legacy SD startup is bypassed in the study profile. Invalid values are null; known capture UTC is separate from receipt time. Routine capture is gated by CONTINUOUS; control capture and backlog upload are independent of that mode.

The study journal uses `/sd/study_sf/vNNNN/<boot-uuid>.jsonl`, checked exclusive creation, append/readback/fsync, bounded batches and exact typed receipt matching. Reboot replays retained records; backend deduplicates. No durable global cursor, no automatic file deletion, no complete-card inventory or zero-loss claim. Full queues, malformed/torn records and storage faults must remain visible. The F24–F26 guards, link anchors and ordered macro/undefine sequence are retained; only the active profile inheritance chain is flattened. Intermediate profile definitions remain test-only in `tests/fixtures/dtn/sf7_intermediate_profiles.ini` so historical contracts can still be checked against actual retained source.

The old F06 historical archive importer is not the Study SF path; its independent implementation and evidence remain at SF7. Historical firmware development documents are available in the pinned source tree (DTN_F24*, DTN_F25*, DTN_F26*, DTN_GAP_AND_ACCEPTANCE.md and STUDY_SF_BUILD_ONLY.md). No private logs or backups are moved or deleted.

## Backend and local evidence

Keep the existing study wire contract, SQL proposals, isolated Edge handler, local reviewer policy, original dashboard study projection and synthetic fixtures. See `backend/docs/study_sf_contract.md` and `backend/docs/study_sf_local_integration.md`. SF6 final local DB: 263 rows; original 250 unchanged; 13 new (8 control, 5 routine), 215 non-fixture rows. Not a formal total generated-vs-persisted-vs-confirmed denominator. Production RLS/legacy grants remain on hold; no hosted schema/function deployment.

## Validation procedure and boundaries

Compare all effective flags and inherited build options to SF7; run retained source/host/Node/SQL tests (fresh disposable local DB only); build the common offline, unconfigured and separately credentialed V003 bench profiles and relevant main regression targets; inspect linked symbols and artifact hashes; verify both enrolled devices preserve NVS and direct SOS/ACK/CANCEL, SD modes/restart, actual GPS/weather behavior, and V003-only local sync/duplicate/conflict behavior. Do not re-enroll or erase existing boards. No charging, PMU, external button, new telemetry or multihop changes in this candidate. Merge only after review and accepted tests.

## Integration validation — 26 November 2026

The consolidated candidate was validated independently of the multihop branch.
The 145-path change set (17 modified, 128 added) retains the operational
closure and regression dependencies; it is deliberately conservative, not a
proof that every retained historical module is essential at runtime. The 51
pre-existing main environments remain available. Four Study SF environments
are active; 29 intermediate profiles are test fixtures, not build targets.

| Gate | Evidence and disposition |
| --- | --- |
| I1 | Selected integration from main; SF7 unchanged; 205 changed paths reviewed, not a branch merge. |
| I2 | 31/31 source and selected host checks passed; two test-only compatibility repairs. |
| I3 | Five credential-free firmware profiles compiled/linked; universal application SHA-256 `53294b7d4be3b81450e29141fef320f959b8351e83708f7a40ea53ab5d561351`. |
| I4A | 45/45 Windows host/mock groups, including 67/67 Node tests and frozen-lockfile offline frontend build. |
| I4B | Actual Linux POSIX journal 175/175 assertions and six byte-exact generated fixtures; separate live local SQL/Auth/Edge/RLS lab: 48 accepted + 48 duplicate confirmations, 48 unique synthetic rows. Protected 263 rows unchanged during the synthetic test. |
| I5 | Same universal application installed on V002 and V003; private backup hashes, partition match and NVS-prefix readbacks verified. Operator reported all ten runtime checks passed, including bidirectional direct RF, recording modes, SOS/CANCEL, restart and OLED. Original runtime serial transcripts unavailable; not independently reviewed. |
| I6 | V003-only physical local authenticated HTTPS: 45/45 exact device/Edge receipts, 218 unique records processed (215 duplicate confirmations, three new records: two GPS and one weather). All original 263 rows unchanged; final 266-row fingerprint `266:a97264e0842ea6bb33e5dbed404cd28c`. |
| I6 restore | Verified universal application and NVS-prefix readback, sync image tail cleared. Subsequent supplied V003 serial log confirms ID 3, OFFLINE_READY, Wi-Fi disabled, SOS_ARMED, two retained prefix records, five successful direct TX operations and relay disabled. |

**Acceptance limitation:** A second physical authenticated restart/replay was
not demonstrated (uptime continued increasing and phase 2 had no receipts).
Native host restart recovery and physical duplicate handling passed, but they
do not substitute for that specific physical test. The operator reported normal
restored behavior and supplied the later serial log. Sensor initialization
failed and GPS had no fix in that log; weather/GNSS accuracy, PDR, endurance,
full-card loss accounting, power-loss durability and maritime field trials
remain separate. Do not represent this as production validation or a
fleet-wide authenticated synchronization deployment. Local study records are
not a proof of complete generated-versus-confirmed accounting.

Reports retained outside Git: I4A `THESIS_DTN_I4A_R2_20261126_021632.txt`
(SHA-256 `418d66c3fcc95a8b616de9b3add52cc0c5f36dcdfe640cf140944cd530409676`),
native I4B `THESIS_DTN_I4B_Native_Journal_PASS_20261126.txt`
(`56b1bf4645c529abd5cc9488515645705d521e8ac6a11a8f1222a33c8da135ec`),
live I4B `thesis-dtn-i4b-fc698ec03bde.txt`
(`8eb7b49328243f32356bc961fa9637ef6c38b2677f9f6c7b78879f1c51556a0b`).
The I5 summaries and I6 R3 recovery summary, private flash/readback and
request/response evidence stay outside Git. No credential, private SD record,
coordinate, raw NVS, key or firmware artifact is included in this record.

## CLOUD4 hosted sync (functional bench acceptance)

`tbeam-supreme-universal-study-cloud` inherits the same universal study
profile and NVS identity, SD journal, SOS_ARMED/CONTINUOUS modes, LoRa and
weather path. Offline profile is unchanged. Optional ignored
`study_sf_cloud_config.generated.h` contains one device's own login, Wi-Fi
and publishable key; no privileged key. Missing/mismatched config leaves
SD recording active and Wi-Fi off. Private images are device-specific in
configuration only, not source/profile; protect them as credentials.
The worker uses verified TLS (ESP-IDF certificate bundle), time acquisition,
password sign-in on reconnect or expiry, RAM-only bearer, 30s auth retry,
and the unchanged journal/receipt contract. No new server or SD format.
Compile and physical tests must pass before release; no lossless or
flash-extraction protection claim. The eight-record queue remains bounded.

## CLOUD4 bench acceptance — 26 November 2026

CLOUD1 deployed migration `20261125213233_cloud1_secure_legacy_and_study_sf`,
secured the five retained legacy tables and registered prototype nodes 2/3.
CLOUD2 deployed the existing authenticated ingestion function, verified real
hosted Auth, role isolation, exact receipts, duplicate and conflict handling.
CLOUD3 published the authenticated Netlify study dashboard; public demo is
separate. Hosted database and frontend commits are ancestors of this change.
CLOUD4A compiled both profiles; source contracts passed 19/19 and 33/33 and
native session policy passed. Offline recovery build flash differed by 16
bytes from the original; no binary-equivalence claim. Private V002/V003
cloud builds each used 105380 RAM and 1484876 flash bytes (reported static
usage), verified app-only readback and preserved the prefix/NVS before boot.
Both enrolled devices authenticated and uploaded to hosted Supabase without
a laptop gateway. V003 recovered from HTTP 403 by fresh authentication;
the cause of that response is unknown. Physical restart replay returned
exact duplicate receipts. V003 stored two GPS no-fix records before Wi-Fi
returned and one invalid-weather record afterward, then delivered all three
after restart. Latest V003 transcript: 27 confirmed batches, 58 accepted
and 50 duplicate identities; all 108 identities matched hosted PostgreSQL.
V002's two receipt identities also matched. Last verified hosted snapshot:
113 rows, including 1 excluded known synthetic fixture; 112 non-fixture
bench records (V002:2, V003:110). This is not complete-card accounting.
Netlify displayed both recording vessels and actual synchronized history;
missing sensor/GPS values remained unavailable and live LoRa link unknown.
Local research database last verified 266 rows; it was not changed here.

Keep credentials and generated headers, ELF/BIN files, raw NVS, flash
backups and serial evidence private. Same firmware source/profile for both
nodes; private device-specific credentials are compiled in, not a common
binary. No privileged key or new enrollment path. Physical credential
extraction resistance, long-offline token lifetime trial, Wi-Fi retry energy,
full-card loss accounting, sustained concurrent capture/upload, field
accuracy, endurance and multihop remain unverified. Wi-Fi retries may emit
ESP_ERR_WIFI_STATE; reconnection succeeded in the observed trial. Restart
replays earlier files because the cursor is RAM-only; no automatic deletion.
Do not mix power optimization or other unrelated improvements into CLOUD4.
