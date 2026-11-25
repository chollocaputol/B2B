# Local study Store-and-Forward integration — 25 November 2026

Parent `2329854c2258f40c2c34480920d49188de0b6280`. Local-only candidate; not authorized for production or physical deployment. The accepted firmware, shared validation handler and ingest SQL are unchanged. The R5 entry point uses native HTTP for server-verified Auth and the same transactional PostgREST RPC; no external npm worker dependency. The additive SELECT-only reviewer policy uses server-managed app_metadata. Service-role credentials remain server-side. The existing Acmac React dashboard is reused at `/?study=1` with local reviewer sign-in, while `/` remains demo. There is no second dashboard component. Local access requires the exact loopback frontend and API URL; external basemap disabled. The finite quiescent snapshot has a 10,000-row limit. Capture time is not receipt time; unknown or future timestamps remain in history but not current indicators. Cross-boot ties are ambiguous, cancellation is an observation rather than rescue completion, and delayed synchronization is not live LoRa evidence. CSV includes exact record identity and payload. No schema migration, cloud, firmware flash or SD action is authorized.

## Evidence (operator-run Windows local lab, synthetic only)

- R5: 45/45 host tests; actual Edge readiness 405, three real Auth sign-ins, authorization and recorder rejection, 48 accepted +48 duplicate confirmations, exactly 48 stored rows, reviewer-only RLS, conflict rollback, verified shutdown.
- Original UI: 28/28 host tests; exact 48 fixture projection; TypeScript + Vite build, unchanged lockfile, operator browser PASS, verified shutdown.
- Positive case: three timed weather and one timed GPS record added via existing RPC, 4/4 exact confirmations/payloads, 52 total, browser operator PASS (29 C, 78%, 1009 hPa, GPS 8.31/124.2, three chart points). Temporary rows removed; original 48-row fingerprint `48:0480787a5072d86b4e3c1c880d378b4d` matched, verified shutdown. No physical sensor or radio evidence.
- Earlier temporary view was superseded, not committed. Its final offline guard interrupted the automated completion marker; subsequent original UI and positive review completed normally.

Reports (SHA-256):
- `THESIS_STUDY_SF_EDGE_LOCAL_20261125_052119_32289b9a.txt` — `E953A5C4F1541FA2DC2D97CBE4A2D1FF71AD7C295A281472FB58EE992FC9BE72`
- `THESIS_SF_ORIGINAL_UI_REVIEW_20261125_060618_6d385009.txt` — `E2A2ADE4EDC3F4780042A6E8EE68C2F1A2630CE0D0CE5B504F54139797CBA660`
- `THESIS_SF_POSITIVE_UI_REVIEW_20261125_062145_eeda5b77.txt` — `FA8971BB19B6A75001F4744B70FC6E26B673E270305B662CA039B29BE9E80716`

## Reproducible host checks

From the repository root with Node 24 (or compatible type stripping):

```powershell
$env:SF_EDGE_ENTRY = (Resolve-Path 'supabase/functions/ingest-study-sf/index.ts').Path
node --experimental-strip-types --test tests/study_sf_http.test.mjs tests/study_sf_edge_adapter.test.mjs tests/study_sf_projection.test.mjs tests/study_sf_exact_fixture.test.mjs frontend/src/data.test.ts frontend/src/studyDashboardData.test.ts
Remove-Item Env:SF_EDGE_ENTRY
```

The six synthetic firmware-generated request fixtures are retained under `tests/fixtures/study_sf/`. The adapter fixture reference and exact-projection test imports are adjusted only for repository layout. The actual local Edge test is destructive to a fresh disposable test dataset only; do not run it against a retained or hosted database. It requires explicit loopback-only lab configuration and local-only test credentials. No secrets or generated build products belong in Git.

Physical SD persistence, device Wi-Fi/TLS, power-loss behavior, field accuracy and emergency latency remain untested. Production security hold remains: legacy public tables previously had RLS disabled and broad anonymous privileges. Local software pass does not authorize deployment. Use separate physical and production gates.

## SF5 physical bench follow-up — 25 November 2026

The earlier sections record the synthetic-only checkpoint. Subsequent controlled local V003 trials verified SD capture, retained records, local authenticated HTTPS synchronization, replay deduplication, and DEV8 recording modes. The dashboard now excludes the exact 48 known fixture identities from the study view and export (not the database), sorts displayed history by newest known capture time, and preserves unknown times at the end. Non-fixture is a dataset classification, not independent proof of authenticity.

Final retained local database: 263 rows, including the original 250 unchanged (`250:7b94887e19c1089272d3b25bdb5d5844` for those identities) and 13 new rows. Final fingerprint `263:96b9a12e294c49af58e28344641bd383`. Browser and exported CSV: 215 non-fixture rows (88 weather, 119 GPS, 4 SOS, 4 CANCEL), 215 unique identities, 50 unknown capture times, 202 earlier non-fixture rows unchanged. The four historical emergency pairs (origin 2 events 8/9/10 and origin 3 event 23) show cancellation observed, not rescue completion. CSV SHA-256 `2acf10f527d83114e97596b147042ad1db85d828fb8b6e7361e6e17a96f1a05d`. CSV data and precise positions are not committed. Frontend 20/20 tests, TypeScript and Vite build passed in the retained lab. Combined V003 target build and application-only upload passed. Windows Time restored and temporary firewall rules removed.

This is functional bench evidence, not a complete microSD inventory, formal zero-data-loss measurement, live shore tracking, RF relay validation or production authorization. The generated lab configuration and credential-bearing binaries must remain outside Git. Field trials and the production security hold remain separate.

## DTN integration regression — 26 November 2026

The earlier SF6 figures above remain historical. The independent DTN
integration repeated the isolated live SQL/Auth/Edge/RLS regression (48
accepted, 48 duplicate confirmations, exactly 48 synthetic rows). The
protected retained 263 rows were unchanged during that test. The later V003
physical authenticated trial processed 218 unique records with 45 exact
firmware/Edge receipts: 215 duplicates and three newly accepted records
(two GPS, one weather). Original 263 rows unchanged; final 266-row digest
`266:a97264e0842ea6bb33e5dbed404cd28c`. The second physical authenticated
restart/replay was not demonstrated. Universal offline firmware restoration
was verified. See [DTN validation](../../firmware/universal/DTN_STORE_AND_FORWARD.md)
for scope and limits. No production deployment or legacy security changes.
