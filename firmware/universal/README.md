# Universal T-Beam Supreme firmware — Study Store-and-Forward

The common offline profile is `tbeam-supreme-universal-study`. It reads the existing NVS identity; it never enrolls or renumbers a device. The separate `tbeam-supreme-fleet-usb-enrollment-build-only` profile and `scripts/fleet/onboard.py` remain the one-time enrollment path for blank boards. See [fleet enrollment](FLEET_ONBOARDING_AND_RECOVERY.md).

See [DTN Store-and-Forward](DTN_STORE_AND_FORWARD.md) for the release profiles, architecture, evidence boundaries and test gates. The older main profiles remain available for compatibility; intermediate F24–F26 profiles are historical test fixtures, not active build targets. New multihop development remains on `feature/firmware-multihop` and is not part of this candidate.

This document describes the tested integration; it does not authorize additional flashes or production deployment. The universal offline profile has no Wi-Fi upload; authenticated synchronization is a separate V003-only isolated lab profile. Never commit generated configuration, credentials, SD evidence or flash backups.

## Historical baseline (superseded)

The following original main README is retained for context only. It describes the old hardcoded Board A baseline, not the enrolled universal Study SF profile above.

> # Phase 5.2.2C-1 — isolated universal firmware baseline
>
> This is a **build-only copy of the current Board A application** and is not yet universal.
> It is a protected starting point for subsequent refactors; no original files changed.
>
> **Do not upload this image to both devices.** The copied radio code still uses
> hardcoded `NODE_ID=0xA1`, `NODE_NAME=Vessel-A`, and Board-A-specific neighbor and
> SOS logic. Flashing it to both vessels would duplicate their network identities.
>
> Next: implement per-device NVS identity provisioning, replace opposite-node
> hardcoding, reconcile per-vessel weather and SOS/display behavior, then run a
> full two-device regression before calling the firmware universal.
