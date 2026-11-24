#!/usr/bin/env python3
"""F24S static scope and default-deny compile contract, NOT physical evidence."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
logger=(root/'firmware/shared/storage/thesis_sd_logger.h').read_text()
port=(root/'firmware/shared/storage/dtn_f24r_guarded_file_port.h').read_text()
header=(root/'firmware/shared/storage/dtn_f24s_temporary_logger_files.h').read_text()
ini=(root/'firmware/universal/platformio.ini').read_text()
checks=[
 ('F24S default OFF','#define THESIS_DTN_F24S_BUILD_ONLY 0' in logger),
 ('F24S requires F24R','THESIS_DTN_F24R_BUILD_ONLY != 1' in logger),
 ('F24S requires F24Q','THESIS_DTN_F24Q_BUILD_ONLY != 1' in logger),
 ('F24S requires F24K denial','THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY != 1' in logger),
 ('F24S requires F06 sandbox','THESIS_F06_SD_SANDBOX != 1' in logger),
 ('F24S excludes independent F23D','THESIS_F23D_SD_EVIDENCE != 0' in logger),
 ('F24S excludes fleet SD','THESIS_F22_FLEET_OPERATIONAL != 0' in logger),
 ('F24S excludes legacy relay','THESIS_LAB_RELAY_TX != 0' in logger),
 ('F24S excludes physical GPIO','THESIS_V003_F13_LIVE_OUTPUTS != 0' in logger),
 ('F24S uses F24R lease adaptor','f24sTemporaryFiles(f24rLegacyFiles)' in logger),
 ('F24S F24R No Physical Witness','static const DTNF24Q::DenyWitness f24rNoPhysicalWitness{};' in logger),
 ('F24S F24R zero lease','static const DTNF24Q::Lease f24rNoLease{};' in logger),
 ('F24S guards Volume via its existing witness', 'guarded_.open(path,mode)' in header),
 ('F24S precludes unsupported mode','if ((!append && !read)' in header),
 ('F24S restricts vessel','p[2]!=\'1\'' in header and 'p[2]!=\'3\'' in header),
 ('F24S restricts session uppercase','ch>=\'A\' && ch<=\'F\'' in header),
 ('F24S preserves directory read-only','if (*p==\'\\0\')return !writing;' in header),
 ('F24S does not expose F23D writer path','"f23d_multihop.jsonl"' not in header),
 ('F24S avoids raw File destructor guarantee','no media/grant is supplied' in header),
 ('F24R intercepts available','int available(){return allow(Action::Read)' in port),
 ('F24R intercepts print(char)','size_t print(char ch){return writer_ && allow(Action::Write)' in port),
 ('F24R intercepts print(str)','size_t print(const char* text){return writer_ && text && allow(Action::Write)' in port),
 ('F24S profile extends F24R','extends = env:tbeam-supreme-f24r-file-handle-deny-build-only' in ini),
 ('F24S profile flag only','-D THESIS_DTN_F24S_BUILD_ONLY=1' in ini),
]
# Exact eight temporary File declarations converted under F24S only.
expected=[
 ('appendRecord','F24STemporaryFile file = f24sTemporaryFiles.open(path, FILE_APPEND);'),
 ('initialize directory','F24STemporaryFile dir = f24sTemporaryFiles.open(DIRECTORY, FILE_READ);'),
 ('prior diagnostics','F24STemporaryFile previous = f24sTemporaryFiles.open(DIAGNOSTICS, FILE_READ);'),
 ('boot verification','F24STemporaryFile verification = f24sTemporaryFiles.open(DIAGNOSTICS, FILE_READ);'),
 ('telemetry audit','F24STemporaryFile telemetryFile = f24sTemporaryFiles.open(TELEMETRY, FILE_READ);'),
 ('SOS audit','F24STemporaryFile sosFile = f24sTemporaryFiles.open(SOS_EVENTS, FILE_READ);'),
 ('history root','F24STemporaryFile root = f24sTemporaryFiles.open(historyRoot, FILE_READ);'),
 ('history preflight','F24STemporaryFile file = f24sTemporaryFiles.open(path, FILE_READ);'),
]
for label,stmt in expected:
 checks.append((f'F24S actual logger {label}',logger.count(stmt)==1 and '#if THESIS_DTN_F24S_BUILD_ONLY == 1\n' in logger))
checks.extend([
 ('Original logger File preserved outside F24S','File dir = interposedSD.open(DIRECTORY, FILE_READ);' in logger),
 ('Legacy directory iteration still raw','while (File entry = directory.openNextFile()) {' in logger),
 ('F24R retained export unchanged','static F24RRetainedFile exportFile;' in logger),
 ('F24R retained history unchanged','static F24RRetainedFile historyFile;' in logger),
 ('F24O/P SOS replay still enabled','THESIS_DTN_F24P_BUILD_ONLY == 1' in logger),
])
for label,passed in checks:
 if not passed:raise AssertionError('F24S source contract FAIL: '+label)
print(f'F24S deny-only source-contract PASS: {len(checks)} checks; eight logger sites; iterator and destructor gaps remain')
