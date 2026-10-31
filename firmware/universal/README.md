# Phase 5.2.2C-1 — isolated universal firmware baseline

This is a **build-only copy of the current Board A application** and is not yet universal.
It is a protected starting point for subsequent refactors; no original files changed.

**Do not upload this image to both devices.** The copied radio code still uses
hardcoded `NODE_ID=0xA1`, `NODE_NAME=Vessel-A`, and Board-A-specific neighbor and
SOS logic. Flashing it to both vessels would duplicate their network identities.

Next: implement per-device NVS identity provisioning, replace opposite-node
hardcoding, reconcile per-vessel weather and SOS/display behavior, then run a
full two-device regression before calling the firmware universal.
