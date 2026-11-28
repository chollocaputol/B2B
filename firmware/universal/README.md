# Universal vessel firmware

The same firmware source supports enrolled T-Beam Supreme vessels. PlatformIO builds for an ESP32-S3 board with the existing 8 MB flash layout.

## Build profiles

| Profile | Purpose |
| --- | --- |
| `tbeam-supreme-offline` | Normal vessel operation and SD recording without hosted synchronization |
| `tbeam-supreme-cloud` | Normal vessel operation plus authenticated shore synchronization |
| `tbeam-supreme-enrollment` | Dedicated, supervised USB identity provisioning; not the normal operating image |

From the repository root, with PlatformIO installed:

```sh
pio run -d firmware/universal -e tbeam-supreme-offline
pio run -d firmware/universal -e tbeam-supreme-cloud
pio run -d firmware/universal -e tbeam-supreme-enrollment
```

These are build commands only. Select and verify an individual board and its existing identity before any authorized upload. Never erase flash or re-enroll a working board merely to update its firmware.

## Cloud configuration

Copy `include/study_sf_cloud_config.example.h` to `include/study_sf_cloud_config.generated.h` in a private build checkout. Set the node number to the enrolled identity, supply the intended Wi-Fi network and that device's own authorized login and Supabase publishable key. The generated header is private and ignored. The existing hosted endpoint is defined in `src/synchronization/dtn_worker.inc`; a different hosted project would require an explicit configuration review.

The example itself is deliberately unusable. Missing or mismatched configuration leaves storage available but cloud synchronization unavailable. Never include a service-role or secret key, actual device login or private compiled image in the repository.

## Source layout

| Path | Responsibility |
| --- | --- |
| `src/main.cpp` | Arduino setup and main loop |
| `src/runtime/` | Shared runtime state and declarations |
| `src/communication/` | LoRa and peer communication |
| `src/weather/` | Weather acquisition and processing |
| `src/display/` | OLED navigation and rendering |
| `src/hardware/` | Initialization, I2C and GNSS services |
| `src/storage/` | Record capture and SD filesystem backend |
| `src/synchronization/` | DTN worker and cloud upload |
| `include/` | Firmware interfaces and policies |
| `../shared/` | Shared communication, hardware and storage contracts |

The `.inc` implementation fragments are intentionally included into their owning translation unit, preserving existing initialization order and shared state.

## Operation

The normal boot recording mode is SOS_ARMED. CONTINUOUS enables routine observations. The OLED provides operational pages and development controls; simulated weather or shore states do not establish actual environmental or geographic measurements. Physical relay and unassigned indicator outputs remain disabled in these release profiles.

Use battery-only operation or USB-only with the battery removed when safe; the unresolved simultaneous battery/USB thermal issue remains outside this release. Power down before changing hardware or storage. Stop if abnormal heating occurs.

See [DTN operation](DTN_STORE_AND_FORWARD.md) and [enrollment and recovery](FLEET_ONBOARDING_AND_RECOVERY.md).
