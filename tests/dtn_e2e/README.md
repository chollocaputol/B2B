# F24L synthetic SOS-only end-to-end host executable

**No hardware, radio, Wi-Fi, HTTP or Supabase access.** Run with Python 3.9+:

```powershell
python tests\dtn_e2e\f24l_sos_e2e.py --repo . --self-test
```

The program verifies the exact source-controlled `VesselPacket` schema by Git blob hash, creates a uniquely named temporary synthetic-only directory and runs separate processes for initialization, offline operation, lost response, duplicate, retry, restart and audit. It never reads/writes the original F06/V002 SD evidence. The fake server writes a JSON file in the temporary test directory: this is **not an HTTPS test**. ACK sidecars are a host crash-model exercise, not SD media guarantees. It tests `PACKET_SOS` (62B) **only**; POSITION v2 normal packets and continuous research logging never enter the DTN queue.

This is not yet integrated into the firmware; complete SOS wire-frame capture is not currently recorded by the historical F06 SOS *event metadata* JSONL. Existing F24K remains the accepted firmware boundary.

If `packet_protocol.h` changes, the test denies rather than assuming an old wire layout. Do not change the hash without another field-by-field source audit.
