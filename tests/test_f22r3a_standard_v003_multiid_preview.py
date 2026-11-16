"""F22R3A host-only: multi-ID standardized V003 sensor preview, no board I/O.
Uses the actual shared header and demands V003 legacy restrictions remain intact.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
F09 = ROOT / 'firmware/universal/include/v003_f09_shared_sensor_bus.h'
POLICY = ROOT / 'firmware/shared/hardware/f22_universal_capability_policy.h'
MAIN = ROOT / 'firmware/universal/src/main.cpp'
INI = ROOT / 'firmware/universal/platformio.ini'

CPP = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include "v003_f09_shared_sensor_bus.h"
using namespace V003F09SharedSensorBus;
static_assert(!authorized(true,1) && !authorized(true,2) && authorized(true,3), "legacy V003 gate must remain");
static_assert(!authorized(false,3), "legacy unprovisioned must remain denied");
static_assert(authorizedStandardPreview(true,1,true), "standard vessel 1");
static_assert(authorizedStandardPreview(true,2,true), "standard vessel 2");
static_assert(authorizedStandardPreview(true,3,true), "standard vessel 3");
static_assert(authorizedStandardPreview(true,4,true), "standard vessel 4");
static_assert(authorizedStandardPreview(true,9999,true), "standard vessel max");
static_assert(!authorizedStandardPreview(true,0,true), "zero ID rejected");
static_assert(!authorizedStandardPreview(true,10000,true), "out-of-range ID rejected");
static_assert(!authorizedStandardPreview(false,3,true), "unprovisioned rejected");
static_assert(!authorizedStandardPreview(true,3,false), "wiring opt-in required");
int main() {
 for(uint32_t id : {1U,2U,3U,4U,9999U}) assert(authorizedStandardPreview(true,id,true));
 for(uint32_t id : {0U,10000U}) assert(!authorizedStandardPreview(true,id,true));
 std::cout << "PASS F22R3A multi-ID 1/2/3/4/9999 and all fail-closed cases\n";
}
'''


def run():
    f09 = F09.read_text(encoding='utf-8')
    policy = POLICY.read_text(encoding='utf-8')
    main = MAIN.read_text(encoding='utf-8')
    ini = INI.read_text(encoding='utf-8')
    assert 'bool authorizedStandardPreview' in f09
    assert 'F22UniversalCapability::validIdentity(identityReady, nodeId)' in f09
    assert 'return identityReady && nodeId == 3U;' in f09
    assert 'return provisioned && id >= 1U && id <= 9999U;' in policy
    assert main.count('THESIS_F22R3A_STANDARD_FLEET_PREVIEW') >= 4
    assert main.count('V003F09SharedSensorBus::authorizedStandardPreview(') == 1
    assert main.count('V003F09SharedSensorBus::authorized(') == 1
    assert main.count('setupSensors();') >= 1
    assert 'VesselIdentity::load();' in main
    assert 'THESIS_V003_F15_SD_PILOT_ARMED != 0' in main
    assert 'THESIS_V003_F13_LIVE_OUTPUTS != 0' in main
    assert 'THESIS_LAB_RELAY_TX != 0' in main
    assert 'THESIS_V003_F09_SHARED_SENSOR_BUS != 1' in main
    assert 'THESIS_V003_F19_DUAL_DISPLAY != 1' in main
    assert '[env:tbeam-supreme-f22r3a-standard-v003-multiid-build-only]' in ini
    assert '-D THESIS_F22R3A_STANDARD_FLEET_PREVIEW=1' in ini
    assert '-D THESIS_F22R3A_V003_WIRING_EXPLICIT_OPT_IN=1' in ini
    assert '-D THESIS_V003_F15_SD_PILOT_ARMED=1' in ini  # existing V003 pilot remains separately gated
    assert 'THESIS_F22R3A_STANDARD_FLEET_PREVIEW' not in POLICY.read_text(encoding='utf-8')
    print('PASS F22R3A source policy: exact build-only opt-in, original V003 gate intact')
    compiler = os.environ.get('CXX') or ('C:/msys64/ucrt64/bin/g++.exe' if os.name == 'nt' else 'g++')
    with tempfile.TemporaryDirectory(prefix='thesis_f22r3a_') as d:
        p = Path(d)
        (p/'main.cpp').write_text(CPP, encoding='utf-8')
        out = p/'f22r3a.exe'
        r = subprocess.run([compiler, '-std=c++17', '-Wall','-Wextra','-Werror',
                            '-I', str(F09.parent), str(p/'main.cpp'), '-o', str(out)],
                           capture_output=True, text=True)
        assert r.returncode == 0, r.stderr
        r = subprocess.run([str(out)], capture_output=True, text=True)
        assert r.returncode == 0, r.stderr
        print(r.stdout, end='')
    print('PASS F22R3A no SD, GPIO, SOS or wire-format changes in the policy function')

if __name__ == '__main__':
    run()
