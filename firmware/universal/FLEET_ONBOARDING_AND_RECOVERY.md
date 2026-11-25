# Fleet enrollment and universal firmware recovery

Updated November 2026 for `chollocaputol/B2B`.
**Bench / thesis project only.** The common offline operational PlatformIO
profile is `tbeam-supreme-universal-study`. Compilation is not field
acceptance. The supervised V002 multihop profile remains on the separate
feature branch.

## The identity model

- The *same standard-wiring universal application* reads an independent
  `vessel-ident` NVS namespace: `nodeId` and matching
  `Vessel-%03u` `label`.
- The runtime identity loader is read-only. A normal firmware-only PlatformIO
  upload should preserve existing identity, but a previous `erase_flash`,
  altered partition scheme, or a separate provisioner may not.
- The special `tbeam-supreme-multihop-v002-supervised-bench` profile (feature branch only) is
  **not** the universal restore image. Do not flash that on V003.
- Keep the existing F05 SOS recovery and `sos-identity` NVS namespaces.
  Never clone NVS from one vessel to another. Only a new, blank replacement
  should receive an old logical vessel number.
- Never power the damaged/shorted V001 again until inspected. Never allow
  two powered boards to use the same live vessel ID.

## Which firmware profile?

From `firmware/universal/platformio.ini`:

- `tbeam-supreme-universal-study`: standard-wiring common offline image,
  identity read from NVS, normal SOS/LoRa/GPS/weather and Study SF journal,
  Wi-Fi upload and new relay forwarding OFF.
- `tbeam-supreme-fleet-usb-enrollment-build-only`: dedicated USB enrollment
  path for a new, empty board; no automatic ID assignment or NVS overwrites.
- `tbeam-supreme-multihop-v002-supervised-bench` (feature branch only): experimental,
  deliberately armed **V002-only** radio relay; not a V003 restore.

The repository's `main` branch is the accepted baseline; the
`feature/firmware-multihop` branch contains experimental development.
For recovery, use an explicit reviewed commit/profile and verify its
firmware image, never an unspecified `latest` target.

## A. Adding a replacement for damaged Vessel-001

1. Leave damaged V001 powered **OFF**. Verify the replacement is healthy,
   hardware/wiring are supported, and the LoRa antenna is attached.
2. Preserve existing V001 SD evidence off-device. Do not copy the damaged
   board's NVS onto the replacement. Plug in only the replacement.
3. Build/upload the reviewed **USB-enrollment** profile to the new board
   only, without `erase_flash`. Check COM port; do not assume COM13
   belongs to the new board.
4. In the 115200 baud USB serial console, send `FLEET STATUS`. It must
   return `STATE=BLANK ID=0 LABEL=UNPROVISION` and a 12-character
   uppercase efuse MAC. If `READY` or `INVALID`, **STOP**; the enroll
   command refuses to overwrite any keys.
5. After verifying that no other powered board uses ID 1, send:
   `FLEET ENROLL 1 <MAC_FROM_THIS_BOARD_STATUS>`.
   The firmware checks exact MAC match, blank NVS, writes the two
   `vessel-ident` keys and verifies readback. It does NOT erase
   unrelated NVS namespaces.
6. Confirm `[FLEET] ENROLLED ... ID=1 LABEL=Vessel-001
   REBOOT_REQUIRED=1`. Reboot and verify:
   `[IDENTITY] READY id=1 label=Vessel-001`.
7. After a clean baseline verification, build/upload the reviewed normal
   universal operational profile, then verify identity, normal telemetry,
   SOS/CANCEL state, Study SF journal path, and OLED. New hardware carries a
   new physical MAC and new SOS history even though the logical vessel ID
   is reused. Archive the old V001 journal separately.

**Important:** `scripts/fleet/onboard.py` with no explicit replacement
workflow deliberately treats IDs 1, 2 and 3 as reserved and will select ID
4 or greater for a blank *additional* board. It does **not** automatically
replace a damaged V001; use the explicit USB `FLEET ENROLL 1` procedure
above only for a truly blank replacement.

For a genuinely new fourth vessel, use the default
`scripts/fleet/onboard.py` preview/apply process and a registry stored
**outside the repository**, after checking for duplicate IDs. Do not
renumber V002 or V003 to fill a missing ID.

## B. Restoring Vessel-003 after an accidental flash

**First do no additional flash until you know which device is attached.**

1. Connect **only** the physically labeled V003, with antenna in place.
   Do not reuse V002's earlier COM13 assumption. Use
   `C:\PlatformIO\penv\Scripts\platformio.exe device list`
   to identify the current COM port. Keep the suspect V001 disconnected.
2. Back up V003's microSD history before further writes; power down before
   removing the card. Retain previous boot logs and, if available, note
   which firmware/profile was accidentally flashed.
3. Try read-only serial monitor:
   `platformio.exe device monitor -p COMxx -b 115200`
   (replace COMxx). Look for
   `[IDENTITY] READY id=3 label=Vessel-003` and
   `[LORA-A] SX1262 initialized`.
   If present, the identity survived. If another ID, `UNPROVISION`,
   boot loops, or no output, **STOP** and investigate the image/NVS first;
   do not enroll, erase NVS, or guess a port.
4. Build the reviewed **normal universal operational** profile from a
   known Git commit. Verify a successful PlatformIO build and exact image.
   To upload ONLY if board, COM port and identity status are confirmed:
   `platformio.exe run -d <repo>\firmware\universal -e
   tbeam-supreme-universal-study -t upload
   --upload-port COMxx`.
   This is a firmware upload, **not** `erase_flash` or a factory image
   write. It does not intend to modify NVS. Physical wire/sensor and SD
   compatibility still need review.
5. Monitor the reboot. Verify `READY id=3`, SX1262 initialization,
   operational OLED, GPS, SOS inactive and fresh V003 Study SF journal.
   Do not claim an SD/sensor/RF PASS from a successful compile alone.

### Relevant commands (read-only)

```powershell
$pio = "C:\PlatformIO\penv\Scripts\platformio.exe"
& $pio device list
# Only after identifying the actual V003 COM port:
& $pio device monitor -p COMxx -b 115200
```

## Evidence and caution

- A successful upload does **not** prove NVS was preserved; verify startup
  identity after the flash.
- Do not copy older vessels' NVS partitions, SOS counters, or entire SD
  directories into a replacement. Do not erase flash or re-enroll a device
  merely to change the HOME label.
- If you accidentally flashed V002's supervised multi-hop profile onto
  V003, its restricted transmitter does not authorize relay for ID 3,
  but restore the standard universal firmware for normal testing.
- A replacement V001 plus V002/V003 restores the planned
  V001 → V002 → V003 test topology; with only V002 and V003, test
  direct link and SOS/SD first. This experiment remains
  unauthenticated lab RF, not a sea-deployable mesh.

For the consolidated DTN functional acceptance and the remaining physical
restart, sensor and field limitations, see [DTN Store-and-Forward](DTN_STORE_AND_FORWARD.md).
