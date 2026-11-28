# Delay-tolerant storage and shore synchronization

## Capture and recording

The vessel records observations in an SD journal under `/sd/study_sf/vNNNN/<boot-uuid>.jsonl`. Records use stable identities within their boot session; invalid observations remain null. Capture UTC is recorded only when known, independently of hosted receipt time.

The routine capture intervals are 30 seconds for GPS, one minute for the micro weather layer and ten minutes for the macro layer. CONTINUOUS enables routine capture, while SOS_ARMED remains the boot default. Emergency/control capture and retained backlog synchronization are independent of routine recording selection.

A bounded eight-record queue feeds the storage worker. The worker owns the study journal on its native SPI3 SD path; LoRa uses its existing controller. Journal creation, append, readback and filesystem checks remain in place.

## Shore admission and upload

The cloud profile uses a designated shore reference with provisional 100 m entry and 150 m exit thresholds. AUTO admission requires a valid, fresh GNSS fix. Admission is not proof of Wi-Fi coverage.

The development-only DTN SYNC control supports AUTO, SIM SHORE and SIM OFFSHORE. Simulation is volatile; returning to operational mode, active SOS or reboot clears the simulation override. The gate controls new network work; an already-blocking HTTPS operation may finish before shutdown.

The device uses Wi-Fi, time acquisition, verified TLS and its own authorized login. Uploads use the existing authenticated ingestion contract. A batch is confirmed only after a matching HTTP success receipt validates the batch, node and ordered record identities, with accepted or identical-duplicate statuses.

## Acknowledged journal reclamation

In the correctly configured hosted profile, confirmed records advance an in-memory journal offset. A journal is eligible for automatic reclamation only after all of its records have been confirmed and a clean end-of-file check verifies the file extent. Reclamation is at file level, not after each partial batch.

A fully acknowledged historical journal can be removed. A drained active journal is recreated empty at the same path while the record encoder continues its existing identity sequence. Offline-only recording does not enable automatic reclamation.

If reset occurs before reclamation, retained records may be sent again using the same identities. The backend deduplicates identical records. Malformed or torn content and reclamation failures must remain visible; unconfirmed records are not intentionally discarded.

There is no durable global replay cursor or complete-card accounting claim. Controlled restart and bench synchronization were exercised before the structural refactor; abrupt power-loss durability, full-card loss accounting and maritime field performance remain unverified. Do not manually delete or format an SD card to force synchronization.
