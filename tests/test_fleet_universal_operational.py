"""F22 common V003-standard firmware source contract (no hardware side effects)."""
from pathlib import Path
import shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
MAIN=(ROOT/'firmware/universal/src/main.cpp').read_text(encoding='utf-8')
INI=(ROOT/'firmware/universal/platformio.ini').read_text(encoding='utf-8')
LOGGER=(ROOT/'firmware/shared/storage/thesis_sd_logger.h').read_text(encoding='utf-8')
POLICY=ROOT/'firmware/shared/hardware/f22_universal_capability_policy.h'


def test():
    profile='[env:tbeam-supreme-fleet-universal-operational-build-only]'
    assert profile in INI
    env=INI.split(profile,1)[1].split('\n[env:',1)[0]
    assert 'extends = env:tbeam-supreme-v003-f19-dual-display-sd-pilot-v003-only' in env
    assert '-D THESIS_F22_FLEET_OPERATIONAL=1' in env
    assert '-D THESIS_FLEET_USB_ENROLLMENT=1' in env
    # OLD V003 profile remains present; no new radio relay or physical outputs.
    assert '[env:tbeam-supreme-v003-f19-dual-display-sd-pilot-v003-only]' in INI
    assert 'THESIS_V003_F13_LIVE_OUTPUTS != 0 || THESIS_LAB_RELAY_TX != 0' in MAIN
    assert 'THESIS_V003_F15_SD_PILOT_ARMED != 1' in MAIN
    # Same standard GPIO21/48 sensor code, no ID==3 restriction in the new branch.
    assert 'f09SensorAuthorized = F22UniversalCapability::validIdentity(' in MAIN
    assert 'f09SensorAuthorized = V003F09SharedSensorBus::authorized(' in MAIN
    assert 'f12IndicatorPreview.begin(f09SensorAuthorized &&\n      F22UniversalCapability::validIdentity(' in MAIN
    assert 'f13Indicators.begin(f09SensorAuthorized &&\n      F22UniversalCapability::validIdentity(' in MAIN
    assert 'F22UniversalCapability::validIdentity(true,VesselIdentity::nodeId())' in MAIN
    # Real logger uses NVS identity, never formats card, creates unique F06 sessions.
    assert 'return VesselIdentity::ready() && VesselIdentity::nodeId()==id &&' in LOGGER
    assert 'F22UniversalCapability::validIdentity(true,id)' in LOGGER
    # Verify the actual mount through the interposition facade and legacy alias.
    assert 'if (!interposedSD.begin(47, sdSPI, 1000000, "/sd", 5, false)) {' in LOGGER
    assert 'static auto& interposedSD = SD;' in LOGGER
    assert 'static DTNF24K::DenySdFacade<File> interposedSD;' in LOGGER
    assert 'static DTNF24W::DenyMountFacade<File> interposedSD;' in LOGGER
    assert 'if (interposedSD.exists(candidate)) continue;' in LOGGER
    assert 'if (!interposedSD.mkdir(candidate)) {' in LOGGER
    assert LOGGER.index('if (interposedSD.exists(candidate)) continue;') < LOGGER.index('if (!interposedSD.mkdir(candidate)) {') < LOGGER.index('strcpy(DIRECTORY, candidate);')
    assert '"/THESIS/F06TEST"' in LOGGER and '"%s/V%03lu"' in LOGGER
    assert 'if (!ready || !isAuthorizedVessel(vesselId) || !hybridState.continuous) return false;' in LOGGER
    assert 'if (!ready || !isAuthorizedVessel(vesselId)) return false;' in LOGGER
    assert 'ERROR LEGACY_V002_ONLY' in LOGGER and 'if (vesselId != 2U)' in LOGGER
    print('PASS: one common SD-enabled profile, V003 wiring, 3+4 IDs, protected history, unchanged SOS/outputs profile')
    cxx=shutil.which('g++')
    assert cxx,'g++ required'
    source=r'''
#include "f22_universal_capability_policy.h"
using namespace F22UniversalCapability;
static_assert(validIdentity(true,1U) && validIdentity(true,2U));
static_assert(validIdentity(true,3U) && validIdentity(true,4U));
static_assert(validIdentity(true,9999U));
static_assert(!validIdentity(false,4U));
static_assert(!validIdentity(true,0U) && !validIdentity(true,10000U));
static_assert(pins(SensorWiring::Shared2148).bmeSda == 21);
static_assert(pins(SensorWiring::Shared2148).dhtScl == 48);
int main(){return 0;}
'''
    with tempfile.TemporaryDirectory(prefix='fleet_common_') as temp:
        path=Path(temp)/'test.cpp';path.write_text(source)
        r=subprocess.run([cxx,'-std=c++17','-Wall','-Wextra','-Werror','-fsyntax-only',
            '-I',str(POLICY.parent),str(path)],capture_output=True,text=True)
        assert r.returncode==0,r.stderr
    print('PASS: common identity policy accepts provisioned IDs 1..9999; rejects blank/invalid')
if __name__=='__main__':test()
