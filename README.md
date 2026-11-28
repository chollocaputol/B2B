# B2B — Maritime vessel monitoring and communication system

A thesis prototype for vessel-to-vessel communication and shore-side monitoring using T-Beam Supreme ESP32-S3 devices, LoRa, GNSS, environmental sensors, an OLED interface, microSD storage, Supabase and a web-based operator dashboard.

## System functions

- Direct LoRa vessel telemetry and peer tracking with adaptive medium access.
- SOS, acknowledgement and cancellation handling.
- Independent macro and micro weather observation layers.
- Local OLED navigation, vessel information and battery status.
- Delay-tolerant recording and authenticated shore synchronization.
- Operator access to synchronized vessel, weather and emergency observations.

The vessel firmware is shared across enrolled devices. Device identity is stored in NVS; private cloud credentials are provisioned per device. Multi-hop relay experiments are not enabled in these operational profiles.

## Repository

| Path | Responsibility |
| --- | --- |
| `firmware/` | Vessel firmware, hardware services and shared protocols |
| `frontend/` | React/TypeScript operator dashboard |
| `supabase/` | Database migrations and authenticated ingestion function |
| `scripts/fleet/onboard.py` | Supervised USB enrollment utility |
| `docs/` | Architecture and system operating references |

## Getting started

1. Read [firmware setup](firmware/universal/README.md) and [enrollment and recovery](firmware/universal/FLEET_ONBOARDING_AND_RECOVERY.md).
2. Select the appropriate firmware profile and build without uploading.
3. Read [store-and-forward operation](firmware/universal/DTN_STORE_AND_FORWARD.md).
4. Configure the [operator dashboard](frontend/README.md) with the project's public URL and publishable key.
5. Review the [architecture and interfaces](docs/README.md).

The existing hosted deployment uses the migrations and Edge Function under `supabase/`. Do not reapply migrations, reset databases or redeploy functions just to build or run the dashboard. Existing deployments must be managed by their authorized administrator.

## Prototype boundaries

This is a research prototype, not a certified maritime distress or weather warning service. Missing GNSS or weather observations remain unavailable rather than being fabricated. Shore simulation is a development control, not proof of geographical coverage. The system does not establish lossless storage, field endurance, full weather accuracy or operational multi-hop performance.

Keep actual passwords, generated headers, local environment files, device images, raw records and development evidence outside the published tree. Do not operate a board with both USB and battery connected while the power/thermal issue remains unresolved; power down before swapping hardware or SD cards.
