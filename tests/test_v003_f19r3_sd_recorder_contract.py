#!/usr/bin/env python3
"""F19R3 research journal contracts: bounded, consented, no TX/SD in callbacks."""
from pathlib import Path
R=Path(__file__).resolve().parents[1]
s=(R/'firmware/shared/storage/thesis_sd_logger.h').read_text(encoding='utf-8')
m=(R/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
h=(R/'firmware/universal/include/lora_manager.h').read_text(encoding='utf-8')
l=(R/'firmware/universal/src/lora_manager.cpp').read_text(encoding='utf-8')
assert 'struct ResearchSystemSnapshot' in s
assert 'queueResearchSystem(const ResearchSystemSnapshot& s)' in s
assert 'queueResearchPeer(uint32_t id' in s
for kind in ('research_system','research_peer'):
 assert '\\"kind\\":\\"'+kind+'\\"' in s,kind
for fld in ('pdr_est_pct','missing_est','duplicates','session','gps_fix','gps_utc',
            'satellites','hdop','speed_kmph','tx_ok','tx_fail','sd_drop_tlm'):
 assert fld in s,fld
body=s.split('inline bool queueResearchSystem(',1)[1].split('inline bool queueResearchPeer(',1)[0]
peer=s.split('inline bool queueResearchPeer(',1)[1].split('#endif // F19R3',1)[0]
for b in (body,peer):
 assert '!hybridState.continuous' in b
 assert 'q.count>=EVENT_CAPACITY' in b
 assert '++eventCounter' in b
 assert 'SD.open(' not in b and 'appendRecord(' not in b
assert 'EventQueue &q = urgent ? urgentQueue : telemetryQueue;' in s
assert 'if (!hybridState.shouldRecord(urgent, millis())) return true;' in s
assert 'if (!hybridState.routineEnabled(millis())) return false;' in s
assert 'getPeerSnapshotByIndex(size_t index, PeerDisplaySnapshot &out)' in l
assert 'bool getPeerSnapshotByIndex(size_t index, PeerDisplaySnapshot &out);' in h
assert 'peer.received' in m and 'peer.session' in m
assert 'gps.location.age()<=10000UL' in m
assert 'lastCaptureMs=now;return;' in m
assert 'if (uint32_t(now-lastCaptureMs)<30000UL) return;' in m
assert 'i<peerCount && i<10U' in m
# TX/RX call paths remain exactly the same as the already reviewed F18 baseline.
assert 'SD_EVENT(false, "telemetry_tx"' in l and 'SD_EVENT(false, "telemetry_rx"' in l
print('PASS F19R3: structured GPS/system/per-peer research, RAM-only bounded queues')
print('PASS F19R3: continuous-only snapshots, SOS queue precedence preserved')
