#!/usr/bin/env python3
"""Read-only F24R source-structure guards; not physical safety evidence."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
logger=(root/'firmware/shared/storage/thesis_sd_logger.h').read_text()
f23d=(root/'firmware/shared/storage/f23d_sd_file_writer.h').read_text()
arb=(root/'firmware/shared/storage/dtn_f24q_cross_writer_arbiter.h').read_text()
port=(root/'firmware/shared/storage/dtn_f24r_guarded_file_port.h').read_text()
ini=((root/'firmware/universal/platformio.ini').read_text()+'\n'+(root/'tests/fixtures/dtn/sf7_intermediate_profiles.ini').read_text())
checks=[
 ('F24R macro default OFF', '#define THESIS_DTN_F24R_BUILD_ONLY 0' in logger),
 ('F24R needs F24Q', 'THESIS_DTN_F24Q_BUILD_ONLY != 1' in logger),
 ('F24R needs F24K DENY', 'THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1' in logger),
 ('F24R forbids physical F23D', 'THESIS_F23D_SD_EVIDENCE != 0' in logger),
 ('F24R forbids physical fleet', 'THESIS_F22_FLEET_OPERATIONAL != 0' in logger),
 ('Retained export declared alternate', 'static F24RRetainedFile exportFile;' in logger),
 ('Retained history declared alternate', 'static F24RRetainedFile historyFile;' in logger),
 ('Export uses guarded volume', 'exportFile = f24rLegacyFiles.open(exportPaths[exportIndex], FILE_READ);' in logger),
 ('History uses guarded volume', 'historyFile = f24rLegacyFiles.open(path, FILE_READ);' in logger),
 ('F23D actual readback guarded type', 'F23D_SD_FILE_TYPE readback=F23D_SD_ACCESS.open' in f23d),
 ('F23D persistent pair guarded type', 'F23D_SD_FILE_TYPE file_,verification_;' in f23d),
 ('F23D default unchanged', '#define F23D_SD_FILE_TYPE File' in f23d),
 ('Arbiter metadata probe rechecks witness', 'bool probe(const Lease& l){return check(l);}' in arb),
 ('Port uses arbiter probe', 'arbiter_.probe(lease_)' in port),
 ('Port checks each read', 'allow(Action::Read)?raw_.read()' in port),
 ('Port checks each write', 'allow(Action::Write)?raw_.write(bytes,length)' in port),
 ('Port checks each flush', 'allow(Action::Flush))raw_.flush()' in port),
 ('Port checks close', 'port_->closeSlot(slot_)' in port),
 ('Port documents destructor gap', 'physical integration is prohibited' in port),
 ('F24R profile inherits F24Q DENY','extends = env:tbeam-supreme-f24q-cross-writer-deny-build-only' in ini),
 ('F24R profile enables only build flag','-D THESIS_DTN_F24R_BUILD_ONLY=1' in ini),
]
for label,ok in checks:
 if not ok:raise AssertionError(f'F24R contract FAIL: {label}')
print(f'F24R source-scoped deny-only invariants PASS: {len(checks)} checks; not physical owner proof')
