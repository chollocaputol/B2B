#!/usr/bin/env python3
"""F19R2 host source contracts: SOS-only weather SD, DEV SOS controls, V003 read-only history."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
m=(R/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
s=(R/'firmware/shared/storage/thesis_sd_logger.h').read_text(encoding='utf-8')
ini=(R/'firmware/universal/platformio.ini').read_text(encoding='utf-8')
assert 'THESIS_V003_F19_DUAL_DISPLAY=1' in ini
assert 'THESIS_V003_F13_LIVE_OUTPUTS != 0' in m and 'THESIS_LAB_RELAY_TX != 0' in m
# F19R3 recording mode controls live exclusively on Development Page 8.
assert 'ThesisSDLogger::hybridSetContinuous(sdHybridMenuCursor == 1);' in m
assert 'oledPage == 7 && sdHybridMenuOpen' in m
assert 'ThesisSDLogger::hybridSetContinuous(false);' not in m
assert 'ThesisSDLogger::hybridSetContinuous(true);' not in m
assert 'sosMenuCount+2' in m and 'sosMenuCount+4' not in m
assert 'SD: SOS ARMED' not in m and 'SD: CONTINUOUS' not in m
assert 'MODE_MENU_OPEN' in m and 'MODE_CONFIRMED=' in m
assert 'SOSControls::pollAction(hasActiveLocalSOS())' in m
# F21R4.2: the old unconditional SOS lock has become a display-only
# distress-priority override with an explicitly time-bounded page browser.
assert 'if (f19ActiveSOS)' in m
assert 'if (f19SOS && f21r42SOSBrowse.forceSOS())' in m
assert 'if (f19Display.operational() || f19SOS)' in m
assert 'const uint8_t opPage=f19SOS ? f21r42SOSBrowse.page() : f19Display.operationalPage();' in m
assert 'GPIO_OUTPUTS=DISABLED' not in m or 'THESIS_V003_F13_LIVE_OUTPUTS != 0' in m
# Both dedicated weather journal writers gate on the true hybrid state.
weather=s.split('inline bool queueWeatherSample(',1)[1].split('inline bool queueMicroOutlook(',1)[0]
micro=s.split('inline bool queueMicroOutlook(',1)[1].split('inline void pollEvents()',1)[0]
for body in (weather,micro):
 assert 'THESIS_V003_F19_DUAL_DISPLAY == 1' in body
 assert 'hybridState.routineEnabled(millis())' in body
 assert body.index('hybridState.routineEnabled(millis())') < body.index('EventQueue& q = telemetryQueue;')
# No altered SOS urgent-event filtering. SOS/tail remains authoritative.
assert 'if (!hybridState.shouldRecord(urgent, millis())) return true;' in s
assert 'hybridState.observeSOS(active, millis())' in s
assert 'hybridState = SDHybrid::State{};' in s
assert 'if (appendRecord(DIAGNOSTICS, record))' in s
# Historical access limited to two vessel IDs and exact fixed-session paths.
assert s.count('vesselId != 2 && vesselId != 3') >= 3
assert 'if (vesselId == 3)' in s and 'LEGACY_V003_DISABLED' in s
assert '/THESIS/F06TEST/V%03lu/%s' in s
assert 'validHistorySession(selector)' in s
assert 'HISTORY_MAX_FILE_BYTES = 1048576UL' in s
assert 'SD.open(path, FILE_READ)' in s
assert 'SDH] LIST_BEGIN scope=V%03lu' in s
assert 'requestHistory(command + 8)' in m
assert 'SD LIST/READ' in m
print('PASS F19R2: SOS_ARMED suppresses both automatic weather journal paths')
print('PASS F19R3: DEV7 SOS-only, DEV8 recording controls; distress overrides')
print('PASS F19R2: V003 read-only bounded session listing/export; V002 guards retained')
