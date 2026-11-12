# Firmware shared-folder structure report

Date: 2026-11-12 (Asia/Manila).
Worktree: `C:/Users/Public/Thesis/SYSTEM_main`.
Branch: `main`. Baseline: `4530432` (merged header-name cleanup).

## Scope

All 31 existing shared headers moved into the four requested categories.
No filenames or public symbols changed. No compatibility copies or additional
include search paths were introduced. Changes are limited to moves, literal
include-path updates, one host-test include, current documentation paths, and this report.

## Header mapping

All paths below are relative to `firmware/shared/`.

| Before | After |
| --- | --- |
| compact_position_v2.h | communication/compact_position_v2.h |
| control_retry_schedule.h | communication/control_retry_schedule.h |
| decentralized_mac.h | communication/decentralized_mac.h |
| device_shadow_receiver.h | communication/device_shadow_receiver.h |
| device_verification_contract.h | communication/device_verification_contract.h |
| f05_sos_recovery.h | storage/f05_sos_recovery.h |
| gps_service.h | hardware/gps_service.h |
| lab_relay_gate.h | relay/lab_relay_gate.h |
| mac_contention_gate.h | communication/mac_contention_gate.h |
| multi_peer_sos.h | communication/multi_peer_sos.h |
| multi_peer_telemetry.h | communication/multi_peer_telemetry.h |
| oled_button_debounce.h | hardware/oled_button_debounce.h |
| packet_protocol.h | communication/packet_protocol.h |
| receive_format_guard.h | communication/receive_format_guard.h |
| relay_authorization_contract.h | relay/relay_authorization_contract.h |
| relay_dryrun_scheduler.h | relay/relay_dryrun_scheduler.h |
| relay_policy.h | relay/relay_policy.h |
| relay_preflight.h | relay/relay_preflight.h |
| relay_queue.h | relay/relay_queue.h |
| relay_receipt_state.h | relay/relay_receipt_state.h |
| relay_tx_readiness.h | relay/relay_tx_readiness.h |
| sd_hybrid_policy.h | storage/sd_hybrid_policy.h |
| sd_probe.h | storage/sd_probe.h |
| tbeam_hardware_service.h | hardware/tbeam_hardware_service.h |
| tbeam_pins.h | hardware/tbeam_pins.h |
| telemetry_phase_guard.h | communication/telemetry_phase_guard.h |
| thesis_sd_logger.h | storage/thesis_sd_logger.h |
| v3_shadow_ledger.h | communication/v3_shadow_ledger.h |
| v3_verification_boundary.h | communication/v3_verification_boundary.h |
| vessel_identity.h | hardware/vessel_identity.h |
| wire_gate.h | communication/wire_gate.h |

## Category decisions

- `hardware/` (5): board pin/PMU/GPS/button services and the device-specific
  read-only NVS identity consumer `vessel_identity.h`. Identity provisioning
  and its TX-blocking checks are unchanged.
- `communication/` (14): wire formats, peer state, MAC/retry/timing policies
  and receive-only verification/shadow state. The shadow ledger is volatile
  receive tracking, not durable storage or a relay authorization source.
- `relay/` (8): forwarding candidate, queue, preflight, scheduling, receipt
  diagnostics, lab restriction and authorization/readiness policies.
- `storage/` (4): SD probe/logger/recording policy and F05 durable recovery.

`telemetry_phase_guard.h` remains preserved, even though it has no active
repository include references. This is an organization pass, not file removal.

## Include dependency inventory

Locations refer to the baseline before the moves. Same-category includes that
resolve to the same basename remain unchanged. Cross-category includes use
explicit relative paths; the universal packet compatibility wrapper is retained.

| Baseline source:line | Before include | After include |
| --- | --- | --- |
| firmware/shared/compact_position_v2.h:6 | packet_protocol.h | packet_protocol.h |
| firmware/shared/device_shadow_receiver.h:4 | v3_shadow_ledger.h | v3_shadow_ledger.h |
| firmware/shared/device_verification_contract.h:7 | v3_verification_boundary.h | v3_verification_boundary.h |
| firmware/shared/gps_service.h:6 | tbeam_pins.h | tbeam_pins.h |
| firmware/shared/lab_relay_gate.h:3 | packet_protocol.h | ../communication/packet_protocol.h |
| firmware/shared/receive_format_guard.h:5 | wire_gate.h | wire_gate.h |
| firmware/shared/relay_authorization_contract.h:3 | relay_tx_readiness.h | relay_tx_readiness.h |
| firmware/shared/relay_dryrun_scheduler.h:4 | relay_queue.h | relay_queue.h |
| firmware/shared/relay_preflight.h:3 | relay_policy.h | relay_policy.h |
| firmware/shared/relay_preflight.h:4 | multi_peer_sos.h | ../communication/multi_peer_sos.h |
| firmware/shared/relay_queue.h:4 | relay_policy.h | relay_policy.h |
| firmware/shared/tbeam_hardware_service.h:8 | tbeam_pins.h | tbeam_pins.h |
| firmware/shared/thesis_sd_logger.h:11 | ../universal/include/weather_manager.h | ../../universal/include/weather_manager.h |
| firmware/shared/thesis_sd_logger.h:20 | sd_hybrid_policy.h | sd_hybrid_policy.h |
| firmware/shared/v3_shadow_ledger.h:5 | v3_verification_boundary.h | v3_verification_boundary.h |
| firmware/shared/v3_verification_boundary.h:9 | wire_gate.h | wire_gate.h |
| firmware/universal/include/packet_protocol.h:3 | ../../shared/packet_protocol.h | ../../shared/communication/packet_protocol.h |
| firmware/universal/src/lora_manager.cpp:1 | ../../shared/tbeam_pins.h | ../../shared/hardware/tbeam_pins.h |
| firmware/universal/src/lora_manager.cpp:2 | ../../shared/vessel_identity.h | ../../shared/hardware/vessel_identity.h |
| firmware/universal/src/lora_manager.cpp:3 | ../../shared/relay_policy.h | ../../shared/relay/relay_policy.h |
| firmware/universal/src/lora_manager.cpp:4 | ../../shared/relay_queue.h | ../../shared/relay/relay_queue.h |
| firmware/universal/src/lora_manager.cpp:5 | ../../shared/relay_dryrun_scheduler.h | ../../shared/relay/relay_dryrun_scheduler.h |
| firmware/universal/src/lora_manager.cpp:6 | ../../shared/relay_preflight.h | ../../shared/relay/relay_preflight.h |
| firmware/universal/src/lora_manager.cpp:7 | ../../shared/relay_receipt_state.h | ../../shared/relay/relay_receipt_state.h |
| firmware/universal/src/lora_manager.cpp:8 | ../../shared/relay_tx_readiness.h | ../../shared/relay/relay_tx_readiness.h |
| firmware/universal/src/lora_manager.cpp:9 | ../../shared/relay_authorization_contract.h | ../../shared/relay/relay_authorization_contract.h |
| firmware/universal/src/lora_manager.cpp:10 | ../../shared/lab_relay_gate.h | ../../shared/relay/lab_relay_gate.h |
| firmware/universal/src/lora_manager.cpp:12 | ../../shared/control_retry_schedule.h | ../../shared/communication/control_retry_schedule.h |
| firmware/universal/src/lora_manager.cpp:14 | ../../shared/compact_position_v2.h | ../../shared/communication/compact_position_v2.h |
| firmware/universal/src/lora_manager.cpp:15 | ../../shared/receive_format_guard.h | ../../shared/communication/receive_format_guard.h |
| firmware/universal/src/lora_manager.cpp:16 | ../../shared/device_verification_contract.h | ../../shared/communication/device_verification_contract.h |
| firmware/universal/src/lora_manager.cpp:17 | ../../shared/device_shadow_receiver.h | ../../shared/communication/device_shadow_receiver.h |
| firmware/universal/src/lora_manager.cpp:34 | ../../shared/multi_peer_telemetry.h | ../../shared/communication/multi_peer_telemetry.h |
| firmware/universal/src/lora_manager.cpp:35 | ../../shared/multi_peer_sos.h | ../../shared/communication/multi_peer_sos.h |
| firmware/universal/src/lora_manager.cpp:49 | ../../shared/f05_sos_recovery.h | ../../shared/storage/f05_sos_recovery.h |
| firmware/universal/src/main.cpp:4 | ../../shared/tbeam_pins.h | ../../shared/hardware/tbeam_pins.h |
| firmware/universal/src/main.cpp:5 | ../../shared/vessel_identity.h | ../../shared/hardware/vessel_identity.h |
| firmware/universal/src/main.cpp:6 | ../../shared/oled_button_debounce.h | ../../shared/hardware/oled_button_debounce.h |
| firmware/universal/src/main.cpp:7 | ../../shared/tbeam_hardware_service.h | ../../shared/hardware/tbeam_hardware_service.h |
| firmware/universal/src/main.cpp:8 | ../../shared/gps_service.h | ../../shared/hardware/gps_service.h |
| firmware/universal/src/main.cpp:18 | ../../shared/decentralized_mac.h | ../../shared/communication/decentralized_mac.h |
| firmware/universal/src/main.cpp:19 | ../../shared/mac_contention_gate.h | ../../shared/communication/mac_contention_gate.h |
| firmware/universal/src/main.cpp:65 | ../../shared/thesis_sd_logger.h | ../../shared/storage/thesis_sd_logger.h |
| firmware/universal/src/main.cpp:73 | ../../shared/sd_probe.h | ../../shared/storage/sd_probe.h |
| tests/test_weather_indicators.py:26 | multi_peer_sos.h | communication/multi_peer_sos.h |

Important cross-folder updates:

- `relay/lab_relay_gate.h` includes `../communication/packet_protocol.h`.
- `relay/relay_preflight.h` includes `../communication/multi_peer_sos.h`.
- `storage/thesis_sd_logger.h` includes
  `../../universal/include/weather_manager.h` when F07 logging is enabled.
- `tests/test_weather_indicators.py` embeds
  `#include "communication/multi_peer_sos.h"`; its existing shared include
  search root and all assertions are unchanged.

## Changed files beyond the 31 header moves

- `firmware/universal/src/main.cpp`: shared include paths only.
- `firmware/universal/src/lora_manager.cpp`: shared include paths only.
- `firmware/universal/include/packet_protocol.h`: wrapper target path only.
- `tests/test_weather_indicators.py`: one embedded C++ include path only.
- `firmware/universal/INDICATOR_REVIEW.md`: two source-reference paths only.
- `FIRMWARE_STRUCTURE_REPORT.md`: this report.

The previous `FIRMWARE_CLEANUP_REPORT.md` is preserved as the historical
review record for the earlier naming-only cleanup; its old paths describe that
checkpoint. Current structure and paths are documented here.

## Behavior and workspace restrictions

No function, class, namespace, constant, algorithm, packet field/layout, threshold
or default is intentionally changed. No changes to weather algorithms, GPS,
LoRa, SOS, relay authorization, microSD logic or scheduling.
The normal fallback `THESIS_LAB_RELAY_TX=0`, relay-disabled readiness/authorization
flags, zero relay budget and hard-locked device trust callbacks remain unchanged.
The three existing lab relay profiles retain their explicit opt-in flags.
Physical LED/buzzer outputs remain disabled, with unassigned GPIOs and unchanged
legacy output restrictions. `platformio.ini` is unchanged.

All source operations use SYSTEM_main only. Git commands use the linked
worktree shared metadata; original SYSTEM working files and experimental weather
files are never inspected or edited. No device flashing, serial-port operation or
microSD access. No staging, commit, push, merge or branch update is authorized.

## Validation results

Baseline: both existing host suites PASS. The weather suite emits the same
two known warnings: `gotBMETempHumidity` set but unused, and enum/non-enum
conditional expression.

Post-change host results:

| Existing suite | Baseline | After reorganization |
| --- | --- | --- |
| `python tests/test_weather_indicators.py` | PASS | PASS |
| `python tests/test_f07_weather_acceptance.py` | PASS | PASS |

The indicator suite retains all assertions, including disabled-output zero GPIO
writes and the expected compile-time rejection of conflicting legacy SOS output
ownership. The weather suite retains all assertions and emits the same two
pre-existing warnings as baseline. No new host-test warning or failure occurred.

All PlatformIO environments were built with the unchanged configuration:

```powershell
$env:PLATFORMIO_CORE_DIR = 'C:/PlatformIO'
& 'C:/PlatformIO/penv/Scripts/platformio.exe' run -d firmware/universal
```

| Environment | Result | Duration |
| --- | --- | --- |
| tbeam-supreme | SUCCESS | 00:01:51.270 |
| tbeam-supreme-lab-v002 | SUCCESS | 00:01:30.627 |
| tbeam-supreme-lab-v002-sd-probe | SUCCESS | 00:01:35.649 |
| tbeam-supreme-lab-v002-sd-logger | SUCCESS | 00:01:36.654 |
| tbeam-supreme-sd-logger | SUCCESS | 00:01:32.891 |
| tbeam-supreme-f05-recovery | SUCCESS | 00:01:31.651 |
| tbeam-supreme-f06-sd-f05 | SUCCESS | 00:01:34.519 |
| tbeam-supreme-f06-sandbox | SUCCESS | 00:01:34.945 |
| tbeam-supreme-f06-sandbox-sd-ui | SUCCESS | 00:01:33.377 |
| tbeam-supreme-f06-sandbox-hybrid | SUCCESS | 00:01:34.079 |
| tbeam-supreme-f07-weather-sandbox | SUCCESS | 00:01:33.243 |

PlatformIO returned 0; all 11 environments succeeded. No environment was skipped
or blocked, and no build failure occurred. There were no upload/monitor targets.
The first builds installed dependencies into ignored local artifact folders;
no platform, dependency version constraint, build flag or firmware profile was edited.
The installed toolchain is Espressif32 55.3.312, Arduino ESP32 3.3.12 and Xtensa
GCC 14.2.0+20260121. PlatformIO emitted its terminal-codepage metrics notice;
firmware size checks and image generation still succeeded.

Hardware/RF behavior was not exercised. No device was flashed, no serial port was
opened, and no microSD card was accessed. Hardware acceptance remains future work.

Final structural review:

- PASS: all 81 baseline tracked files compared byte-for-byte against
  `git cat-file --filters HEAD:<old-path>` (preserving Windows checkout line
  endings), allowing only the documented include/reference path substitutions
  and the 31 header moves.
- PASS: 28 moved headers are byte-identical; only `relay/lab_relay_gate.h`,
  `relay/relay_preflight.h` and `storage/thesis_sd_logger.h` have content changes,
  each limited to one include path.
- PASS: every quoted repository include resolves, including conditionally
  compiled firmware and embedded host-test includes. No extra include search
  path was introduced. Same-category basename includes remain valid.
- PASS: exactly 31 headers exist under the four categories, with no duplicate
  headers or flat-root compatibility copies.
- PASS: 32 include-path substitutions and two current-documentation source-path
  substitutions, with all other baseline bytes unchanged.
- PASS: changed-file audit has 37 logical files: 31 header moves, five reference
  files and this new report. No unrelated tracked file change or untracked source
  addition is present. The prior cleanup report is unchanged.
- PASS: `git diff --check` and report whitespace checks. Existing header newline
  formatting is preserved, including the baseline files listed below that lack
  a final newline.
- PASS: no staged changes. `main` and HEAD remain at `4530432`. No commit, push,
  merge or branch update has been performed.

Baseline missing-final-newline formatting intentionally preserved:

- `firmware/shared/storage/sd_probe.h`

Because the moves are unstaged, ordinary Git status lists the original paths as
deleted and the destination folders as untracked. The complete mapping and byte
audit confirm that every header is preserved at its destination.

No firmware behavior was intentionally changed. The local reorganization is
ready for review; it has not been committed or pushed.


Build and audit details are retained locally in the ignored
`.pio_structure_validation/` folder; generated firmware remains in ignored `.pio/`.

## Future work order (unchanged)

1. LED/buzzer hardware integration.
2. Independent dual-layer weather implementation.
3. Multi-hop relay validation.
4. Final firmware acceptance testing.
