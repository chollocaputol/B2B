"""F15 static preflight: preserve existing weather and SD isolation contracts.
No firmware/SD card interaction. Host C++/PlatformIO compilation is separate.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
MAIN=(ROOT/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
INI=(ROOT/'firmware/universal/platformio.ini').read_text(encoding='utf-8')
LOGGER=(ROOT/'firmware/shared/storage/thesis_sd_logger.h').read_text(encoding='utf-8')

assert 'THESIS_V003_F15_MACRO_SD_SANDBOX' in MAIN
assert 'VesselIdentity::nodeId() == 3U' in MAIN
assert 'F15 requires Vessel-003 real F09-F11' in MAIN
assert 'THESIS_V003_F15_MACRO_SD_SANDBOX != 1)' in MAIN
assert 'SD permitted only by guarded F15 sandbox' in MAIN
assert 'f15QueuePhysicalMacroMedian();' in MAIN
assert 'if (finished) {' in MAIN
assert MAIN.index('if (finished) {') < MAIN.index('f15QueuePhysicalMacroMedian();')
assert '#if THESIS_F07_WEATHER_LOG == 1 && THESIS_V003_F11_MACRO_FIVE_MEDIAN != 1' in MAIN
assert "weatherMeasurement.gpsUtcIso8601[0] = '\\0';" in MAIN
assert 'gps.date.age() <= 10000UL && gps.time.age() <= 10000UL' in MAIN
assert 'weatherMeasurement.gpsUtcValid = (n == 20)' in MAIN
assert 'ThesisSDLogger::queueWeatherSample(weatherMeasurement)' in MAIN
assert 'F15-WX' in MAIN and 'F15-SD' in MAIN
assert '[env:tbeam-supreme-v003-f15-macro-sd-sandbox-build-only]' in INI
f15_env=INI.split('[env:tbeam-supreme-v003-f15-macro-sd-sandbox-build-only]',1)[1]
for flag in ('THESIS_SD_LOGGER=1', 'THESIS_SD_FLEET=1',
             'THESIS_F05_RECOVERY=1', 'THESIS_F06_SD_SANDBOX=1',
             'THESIS_F07_WEATHER_LOG=1', 'THESIS_V003_F15_MACRO_SD_SANDBOX=1'):
    assert flag in f15_env,flag
for flag in ('THESIS_V003_F13_LIVE_OUTPUTS=1', 'THESIS_LAB_RELAY_TX=1',
             'THESIS_V003_F14_INDICATOR_PROBE=1'):
    assert flag not in f15_env,flag
assert '/THESIS/F06TEST' in LOGGER and 'if (SD.exists(candidate)) continue;' in LOGGER
assert 'if (!SD.begin(47, sdSPI, 1000000, "/sd", 5, false))' in LOGGER
assert 'if (s.gpsUtcValid && strlen(s.gpsUtcIso8601) == 20)' in LOGGER
assert '"null"' in LOGGER
assert 'inline bool queueWeatherSample(const WeatherMeasurement& s)' in LOGGER
print('PASS F15 source: completed macro median to RAM SD queue; no extra boot weather sample')
print('PASS F15 source: Vessel-003 gate, unique SD sandbox, UTC-valid/null, no physical outputs/relay')
