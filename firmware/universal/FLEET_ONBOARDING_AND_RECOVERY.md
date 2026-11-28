# Fleet enrollment and firmware recovery

Each vessel has a unique enrolled identity stored in NVS. Normal offline and cloud firmware use that existing identity; they do not authorize new enrollment.

## New device

Use the dedicated `tbeam-supreme-enrollment` profile only for supervised provisioning. The enrollment utility is `scripts/fleet/onboard.py`; run `python scripts/fleet/onboard.py --help` to inspect its arguments and prerequisites. The utility requires Python with pyserial and the correct USB serial port and local registry.

Start with a read-only status/preview. Verify the physical board, its eFuse MAC, unique intended vessel ID and registry assignment. Do not proceed if the device is already enrolled, inconsistent or invalid. Enrollment must be an explicit, operator-confirmed action; do not apply it to an already working board. After enrollment, confirm the identity after reboot before installing the reviewed normal operational profile.

## Existing device or recovery

1. Power down safely and verify the intended physical device and serial port.
2. Use a known reviewed source and image, and preserve private recovery material.
3. Check identity, current boot state and hardware layout before an authorized application update.
4. Preserve NVS, partition table and bootloader. Do not use a full-chip erase, clone another vessel's NVS, or format its SD card.
5. After an update, verify the same vessel identity, ordinary radio and OLED behavior, recording and (for cloud devices) authenticated synchronization.

A normal application update is not a registration or identity change. Recovery is device-specific; do not guess flash offsets or reuse another vessel's credentials or backups.

Do not connect a battery and USB together while the power/thermal issue remains unresolved. Keep private backups and credential-bearing binaries outside the repository.
