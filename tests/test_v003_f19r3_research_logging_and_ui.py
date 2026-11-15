#!/usr/bin/env python3
"""F19R3 contracts: weather disclaimer, dedicated DEV7 SOS, DEV8 SD recorder."""
from pathlib import Path
R=Path(__file__).resolve().parents[1]
m=(R/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
r=m.split('// 128x64 landscape, six rows of 6x10 font.',1)[1].split('#endif // F19 renderer',1)[0]
assert 'u8g2.drawStr(0,59,"WEATHER GUIDANCE ONLY")' in r
assert 'NOT SAFETY ALL-CLEAR' not in r
assert 'sosMenuCount+4' not in m and 'sosMenuCount+2' in m
assert 'SD: SOS ARMED' not in m and 'SD: CONTINUOUS' not in m
assert 'ThesisSDLogger::hybridSetContinuous(sdHybridMenuCursor == 1)' in m
assert '[OLED-SD] MODE_CONFIRMED=%s' in m
assert 'sdStorageMenuCursor == 2' not in m or 'MODE_MENU_OPEN' in m
assert '[F19R3-RESEARCH] system_queued=' in m
assert 'f19r3CaptureResearch30s();' in m
assert 'SOSControls::pollAction(hasActiveLocalSOS())' in m
assert 'THESIS_V003_F13_LIVE_OUTPUTS != 0' in m and 'THESIS_LAB_RELAY_TX != 0' in m
print('PASS F19R3: correct OP weather safety disclaimer and DEV7/DEV8 separation')
