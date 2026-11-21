# Universal T-Beam Supreme firmware

The historical Phase 5.2.2C-1 copy was build-only and **not yet universal**.
Subsequent commits added NVS vessel identity, the standardized fleet
operational profile, SOS/ACK/CANCEL, GPS/weather/OLED, F06 SD sandboxing,
and experimental guarded relay candidates. Do **not** deploy an arbitrary
PlatformIO environment or assume build-only means hardware accepted.

**New board and recovery instructions:** see
[**Fleet enrollment and universal firmware recovery**](./FLEET_ONBOARDING_AND_RECOVERY.md).

The normal common-fleet profile is
`tbeam-supreme-fleet-universal-operational-build-only`. It loads the
device's existing `vessel-ident` NVS keys and does not automatically
assign a new vessel number.

The USB enrollment-only profile is
`tbeam-supreme-fleet-usb-enrollment-build-only`.
The external utility is `scripts/fleet/onboard.py`; its default allocator
reserves IDs 1–3 and assigns ID 4+ for an additional blank device.
Replacing a dead V001 is a different procedure described in the guide.

The physical multihop laboratory profile
`tbeam-supreme-multihop-v002-supervised-bench` is intentionally separate.
It must **never** be used to restore V003 or as normal universal fleet
firmware. V001-V002-V003 multihop transmission remains subject to
independent hardware validation.
