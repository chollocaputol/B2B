# Firmware shared-header cleanup review

Date: 2026-11-12 (Asia/Manila).
Worktree: `C:/Users/Public/Thesis/SYSTEM_cleanup`.
Branch: `chore/clean-shared-headers`.
Baseline: `feature/microsd-event-logging`, `90c9235`.

## Scope and filename mapping

Completed: five header renames and six include-filename substitutions.
All headers remain in `firmware/shared/`. Names reflect inspected contents;
legacy namespaces, comments, declarations, constants and implementations stay intact.

| Before | After | Inspected responsibility |
| --- | --- | --- |
| phase5_1_6_rx_guard.h | receive_format_guard.h | Receive-only format classification, without authentication or relay permission |
| phase5_1_7_verify_boundary.h | v3_verification_boundary.h | Portable v3 verification policy with caller-supplied trusted-key and Ed25519 callbacks |
| phase5_1_8_device_backend.h | device_verification_contract.h | Unavailable device-verification contract; empty callbacks and false authorization flags |
| phase5_1_9_shadow.h | v3_shadow_ledger.h | Volatile per-origin v3 shadow lifecycle; no operational SOS/ACK/relay authority |
| phase5_1_9D_shadow_rx.h | device_shadow_receiver.h | Device shadow receive adapter; libsodium verifier remains locked behind a rejecting trusted-key callback |

## Complete pre-change filename-reference inventory

Search covered tracked repository files and hidden/ignored source files before
PlatformIO generated its dependency/build directories. Includes, tests, scripts,
configuration and documentation were included. Exactly six references existed:

| Referencing file | Original line | Original filename |
| --- | --- | --- |
| firmware/universal/src/lora_manager.cpp | 15 | phase5_1_6_rx_guard.h |
| firmware/universal/src/lora_manager.cpp | 16 | phase5_1_8_device_backend.h |
| firmware/universal/src/lora_manager.cpp | 17 | phase5_1_9D_shadow_rx.h |
| firmware/shared/phase5_1_8_device_backend.h | 7 | phase5_1_7_verify_boundary.h |
| firmware/shared/phase5_1_9_shadow.h | 5 | phase5_1_7_verify_boundary.h |
| firmware/shared/phase5_1_9D_shadow_rx.h | 4 | phase5_1_9_shadow.h |

No filename references existed in tests, scripts, configuration or documentation.
This report intentionally records the original filenames for review.
`wire_gate.h` is an unchanged dependency of the receive guard and verification boundary.

## Changed-file list

- The five header renames in the mapping above (three also receive one include substitution each).
- `firmware/universal/src/lora_manager.cpp`: three include-filename substitutions.
- `FIRMWARE_CLEANUP_REPORT.md`: this inventory, classification and validation report.

The intended result is seven logical changed files, counting each rename once.
Renames are left unstaged; ordinary Git status can show old paths as missing and
new paths as untracked until a reviewer chooses to stage them.

## Intentionally preserved files and classifications

All other tracked files are preserved. In particular:

- Active relay chain: `relay_policy.h`, `relay_queue.h`,
  `relay_dryrun_scheduler.h`, `relay_preflight.h`, `relay_receipt_state.h`,
  `relay_tx_readiness.h`, `relay_authorization_contract.h`, `lab_relay_gate.h`.
  All have repository include references; historical phase labels are not a reason to remove them.
- Active shared infrastructure: `compact_position_v2.h`, `control_retry_schedule.h`,
  `decentralized_mac.h`, `gps_service.h`, `mac_contention_gate.h`,
  `multi_peer_sos.h`, `multi_peer_telemetry.h`, `oled_button_debounce.h`,
  `packet_protocol.h`, `tbeam_hardware_service.h`, `tbeam_pins.h`,
  `vessel_identity.h`, `wire_gate.h`.
- Active opt-in recovery/storage: `f05_sos_recovery.h`, `sd_hybrid_policy.h`,
  `sd_probe.h`, `thesis_sd_logger.h`. F05 names a specific recovery experiment;
  its header is conditionally included and is outside this rename scope.
- `firmware/universal/include/packet_protocol.h`: active compatibility wrapper
  forwarding to the shared packet definition; retain.
- `firmware/universal/include/physical_indicators.h`, `sos_controls.h`,
  `weather_indicator_policy.h`, `weather_manager.h`, `src/main.cpp`,
  `src/weather_manager.cpp`, `platformio.ini`, `INDICATOR_REVIEW.md`, both
  existing host test scripts: unchanged.
- `scripts/sd_tools/usb_export.py`, `usb_history_export.py`: device export tools,
  not proven obsolete. Both contain serial-device operations; inspected only,
  never executed during cleanup.

### Remaining cleanup candidates (no automatic deletion)

| File | Classification and evidence | Follow-up |
| --- | --- | --- |
| firmware/shared/telemetry_phase_guard.h | Unreferenced helper in this checkout; no filename or namespace references elsewhere. Contains real peer-relative timing logic; phase describes timing, not just a milestone. | Review historical intent before deciding whether to retain, test or remove. |
| firmware/universal/README.md | Stale baseline documentation: describes hardcoded Board A identity and future provisioning, while current main loads VesselIdentity. | Rewrite separately after confirming current deployment constraints. |
| firmware/shared/wire_gate.h | Active classifier with stale HOST-ONLY / not wired into ESP32 comment; included transitively from lora_manager.cpp. | Correct documentation separately; preserve active code. |
| firmware/shared/relay_dryrun_scheduler.h, relay_preflight.h | Active helpers with historical logging-only comments; current lora_manager.cpp has an explicit opt-in lab TX path. | Review comments against bench-only integration in a separate documentation change. |
| firmware/universal/src/main.cpp | MAC_CAD_MODE comment labels mode 1 default while the fallback definition is 0. | Reconcile documentation separately without changing scheduling defaults. |
| frontend/app/page.tsx | Empty placeholder; current frontend entry is src/main.tsx under Vite and tsconfig includes src. | Confirm whether abandoned Next scaffold is wanted before removal. |
| frontend/components/vesselmap.tsx | Empty placeholder outside current src tree. | Confirm intended future use before removal. |
| backend/README.md | Empty documentation placeholder. | Populate or remove only in a separate approved cleanup. |

## Behavior preservation

No functionality changes are intended. Retain all `Phase516`, `Phase517`,
`Phase518`, `Phase519`, `Phase519D` names and every other public symbol.
The default `THESIS_LAB_RELAY_TX=0`, relay readiness/authorization false flags,
zero relay budget, device key hard lock, `kOutputsEnabled=false`, unassigned
indicator pins and legacy disabled SOS outputs are unchanged.
No SOS, LoRa, GPS, relay authorization, storage, packet, weather threshold or
scheduling logic is changed.

Only the cleanup worktree's source files are inspected/edited. Git commands
issued here necessarily use this linked worktree's shared Git metadata; no
original SYSTEM working-tree files are inspected or edited.
No reset, clean, restore, staging, commit, push, merge, branch edits, firmware
upload/flashing or serial/microSD operations are performed.

## Validation results

Baseline host tests: both PASS. Weather test emits two existing warnings:
`gotBMETempHumidity` set but unused, and enum/non-enum conditional expression.
Both host test suites passed before and after cleanup:

- `python tests/test_weather_indicators.py`: PASS, including zero physical GPIO
  writes with disabled outputs and the expected compile-time rejection of legacy
  output ownership conflicts.
- `python tests/test_f07_weather_acceptance.py`: PASS, with the same two warnings
  before and after cleanup. No new host-test warnings or failures.

PlatformIO build-only results (no upload target):

| Environment | Baseline | After cleanup |
| --- | --- | --- |
| tbeam-supreme | PASS (122.949 s) | PASS (21.325 s) |
| tbeam-supreme-lab-v002 | PASS (93.746 s) | PASS (20.750 s) |
| tbeam-supreme-lab-v002-sd-probe | PASS (95.825 s) | PASS (20.987 s) |
| tbeam-supreme-lab-v002-sd-logger | PASS (95.078 s) | PASS (21.434 s) |

Command used before and after:

```powershell
$env:PLATFORMIO_CORE_DIR = 'C:/PlatformIO'
& 'C:/PlatformIO/penv/Scripts/platformio.exe' run -d firmware/universal -e tbeam-supreme -e tbeam-supreme-lab-v002 -e tbeam-supreme-lab-v002-sd-probe -e tbeam-supreme-lab-v002-sd-logger
```

Installed toolchain: Espressif32 55.3.312, Arduino ESP32 3.3.12,
Xtensa GCC 14.2.0+20260121. Existing platform/dependency constraints were not
modified. First builds installed dependencies into ignored local build folders.
PlatformIO reports a terminal-codepage metrics notice in both runs; size checks
and firmware image generation succeed. Firmware RAM/flash sizes observed for
normal, SD probe and SD logger profiles are unchanged before/after.

No baseline or new build failures occurred. No selected host test or build was
blocked. Other seven non-relay storage/recovery/weather profiles were not built;
validation selected the normal profile and every profile enabling lab relay.
Frontend/backend checks are outside this firmware-only change. Hardware, RF,
flashing and microSD acceptance checks were deliberately not run; build success
does not substitute for the future hardware acceptance work.

Final review:

- PASS: all 80 tracked baseline files compared byte-for-byte against
  `git cat-file --filters 90c9235:<path>` (honoring Windows checkout line endings),
  allowing only the five path renames and six literal filename substitutions.
- PASS: no original filename references remain in source/tests/scripts/config/docs;
  only this report intentionally retains the historical filename inventory.
- PASS: changed-file audit contains exactly the five header renames,
  `lora_manager.cpp`, and this report. No unrelated additions or source changes.
- PASS: `git diff --check`; new untracked headers and report also checked for
  trailing whitespace and final newlines.
- PASS: index remains unstaged; branch remains `chore/clean-shared-headers`,
  HEAD remains `90c9235`. No commit, push, merge or branch update.

The cleanup is local and ready for review. No firmware behavior was intentionally changed.

## Future work order (unchanged)

1. LED/buzzer hardware integration.
2. Independent dual-layer weather implementation.
3. Multi-hop relay validation.
4. Final firmware acceptance testing.
