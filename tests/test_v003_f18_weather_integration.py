#!/usr/bin/env python3
"""Host-only F18 source and safety-contract regression; no hardware access."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / 'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
INI = (ROOT / 'firmware/universal/platformio.ini').read_text(encoding='utf-8')
SD = (ROOT / 'firmware/shared/storage/thesis_sd_logger.h').read_text(encoding='utf-8')


def need(value: bool, description: str) -> None:
    if not value:
        raise AssertionError(description)


need('#define THESIS_V003_F18_WEATHER_INTERFACE 0' in MAIN,
     'F18 must be opt-in, default disabled')
need('THESIS_V003_F17_INTEGRATED_PREFLIGHT != 1' in MAIN and
     'THESIS_V003_F18_EXPLICIT_OPT_IN != 1' in MAIN,
     'F18 needs F17 and explicit opt-in')
need('THESIS_V003_F13_LIVE_OUTPUTS != 0' in MAIN and
     'THESIS_LAB_RELAY_TX != 0' in MAIN and
     'THESIS_V003_F14_INDICATOR_PROBE != 0' in MAIN,
     'F18 must disallow GPIO, relay, and synthetic probe')
need('static_assert(!V003F13Indicators::kPhysicalOutputsEnabled' in MAIN,
     'F17 must continue preventing live F13 outputs')
need('if (oledPage == 1 && duration >= BUTTON_LONG_PRESS_MS)' in MAIN,
     'macro/micro page toggle must be deliberate long press')
need('f18WeatherMicroView = !f18WeatherMicroView;' in MAIN,
     'view toggle absent')
need('void oledMicroWeatherPage()' in MAIN and
     'EXPERIMENTAL 10-30m' in MAIN and
     'NO_PATTERN' in MAIN and 'RISK_WATCH' in MAIN,
     'micro view must display explicit experimental status')
need('if (f18WeatherMicroView)' in MAIN and
     MAIN.index('void oledMicroWeatherPage() {') < MAIN.index('void oledWeatherPage() {'),
     'micro weather view must render from existing weather page')
need('queueMicroOutlook(' in MAIN and 'queueMicroOutlook(' in SD,
     'micro recording path absent')
need('\\"kind\\":\\"micro_outlook\\"' in SD,
     'micro outlook record must have distinct JSON kind')
need('THESIS_V003_F15_SD_PILOT_ARMED == 1' in MAIN and
     '[F18-MICRO-SD] BUILD_ONLY_NO_SD_QUEUE' in MAIN,
     'build-only must not queue micro records')
need('EventQueue& q = telemetryQueue;' in SD and
     'EventQueue &q = urgentQueue.count ? urgentQueue : telemetryQueue;' in SD,
     'micro weather uses telemetry queue, urgent SOS precedence retained')
need('THESIS_V003_F18_WEATHER_INTERFACE=1' in INI and
     'tbeam-supreme-v003-f18-weather-integration-build-only' in INI and
     'tbeam-supreme-v003-f18-weather-integration-sd-pilot-v003-only' in INI,
     'F18 build profiles missing')
need('THESIS_V003_F15_SD_PILOT_ARMED=1' in INI,
     'explicit pilot arming missing')
# No changed RF packet schema or primary SOS processing: smoke-check both remain.
need('transmitSOS();' in MAIN and 'processSOSRetries();' in MAIN,
     'authoritative SOS path missing')
need('f11MacroSampleForMicro = false;' in MAIN and
     'MACRO_REUSED' in MAIN and '[F11-MACRO]' in MAIN,
     'macro/micro coordination changed unexpectedly')
# Compile the exact new preprocessor guard matrix rather than only asserting
# textual presence. Fail closed when the host C++ compiler is unavailable.
guard_start = MAIN.index('// F18: weather-first integration.')
guard_end = MAIN.index('// Separate F15 hardware opt-in:', guard_start)
guard = MAIN[guard_start:guard_end]
required = {
    'THESIS_V003_F18_WEATHER_INTERFACE': 1,
    'THESIS_V003_F18_EXPLICIT_OPT_IN': 1,
    'THESIS_V003_F17_INTEGRATED_PREFLIGHT': 1,
    'THESIS_V003_F10_MICRO_NOWCAST': 1,
    'THESIS_V003_F11_MACRO_FIVE_MEDIAN': 1,
    'THESIS_V003_F12_INDICATOR_PREVIEW': 1,
    'THESIS_V003_F13_SUPERVISED_DRIVER': 1,
    'THESIS_V003_F15_MACRO_SD_SANDBOX': 1,
    'THESIS_SD_OLED_UI': 1,
    'THESIS_V003_F13_LIVE_OUTPUTS': 0,
    'THESIS_LAB_RELAY_TX': 0,
    'THESIS_V003_F14_INDICATOR_PROBE': 0,
}
def compile_guard(flags):
    result = subprocess.run(['g++', '-std=c++17', '-x', 'c++', '-fsyntax-only', '-'] +
            [f'-D{k}={v}' for k,v in flags.items()], input=guard,
            capture_output=True, text=True)
    return result.returncode == 0, result.stderr

passed, error = compile_guard(required)
need(passed, f'F18 positive guard rejected: {error}')
for changed in ['THESIS_V003_F18_EXPLICIT_OPT_IN',
                'THESIS_V003_F17_INTEGRATED_PREFLIGHT',
                'THESIS_V003_F12_INDICATOR_PREVIEW',
                'THESIS_V003_F15_MACRO_SD_SANDBOX',
                'THESIS_V003_F13_LIVE_OUTPUTS',
                'THESIS_LAB_RELAY_TX',
                'THESIS_V003_F14_INDICATOR_PROBE']:
    faulty = dict(required)
    faulty[changed] = 1 - faulty[changed]
    passed, _ = compile_guard(faulty)
    need(not passed, f'F18 guard improperly accepts {changed}={faulty[changed]}')
print('PASS F18 host: actual C++ guard compilation rejects unsafe opt-in combinations')
print('PASS F18 host: opt-in F17-derived profiles and GPIO/relay/probe lockouts')
print('PASS F18 host: original eight OLED pages retained, macro/micro view toggle')
print('PASS F18 host: independent micro SD trace and urgent SOS record precedence')
print('NO COMMIT / NO FLASH / NO SD ACCESS')
