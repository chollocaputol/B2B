"""F22R2 passive observer: fail-closed real-source checks and host C++ probe matrix."""
from pathlib import Path
import subprocess,tempfile,sys,shutil
ROOT=Path(__file__).resolve().parents[1]
MAIN=(ROOT/'firmware/universal/src/main.cpp').read_text('utf-8')
INI=(ROOT/'firmware/universal/platformio.ini').read_text('utf-8')
POLICY=ROOT/'firmware/shared/hardware/f22_universal_capability_policy.h'
assert POLICY.is_file()
assert MAIN.count('THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER')>=5
assert MAIN.count('F22UniversalCapability::evaluate(f22Observed)')==1
assert '#include "../../shared/hardware/f22_universal_capability_policy.h"' in MAIN
assert 'f22Observed.provisionedIdentity=VesselIdentity::ready();' in MAIN
assert 'f22Observed.vesselId=VesselIdentity::nodeId();' in MAIN
assert 'f22Observed.bme680Responded=bmeAvailable;' in MAIN
assert 'f22Observed.dht20Responded=dhtAvailable;' in MAIN
assert 'F22UniversalCapability::SensorWiring::Shared2148' in MAIN
assert 'F22UniversalCapability::SensorWiring::LegacySplit' in MAIN
assert 'board_review=UNKNOWN' in MAIN
start=MAIN.index('#if THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER == 1\n  // Observe')
end=MAIN.index('\n#endif\n#line ',start)
block=MAIN[start:end]
for unsafe in ('SD.begin(', 'pinMode(', 'digitalWrite(', 'Wire.begin(', 'setupSensors(', 'VesselIdentity::load(', 'radio.', 'Preferences(', 'VesselIdentity::provision('):
    assert unsafe not in block, 'Unexpected observer side effect: '+unsafe
assert 'boardRevisionReviewed=' not in block and 'sensorPinsReviewed=' not in block
assert 'sdCardPresent=' not in block and 'sdExplicitOptIn=' not in block
assert 'f22Observed.sensorWiring=' in block
assert 'F22UniversalCapability::evaluate(f22Observed)' in block
assert 'THESIS_V003_F15_SD_PILOT_ARMED != 0' in MAIN
assert 'THESIS_V003_F13_LIVE_OUTPUTS != 0' in MAIN
assert 'THESIS_LAB_RELAY_TX != 0' in MAIN
assert '#define THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER 0' in MAIN
name='tbeam-supreme-f22r2-passive-observer-v003-build-only'
assert INI.count('[env:'+name+']')==1
conf=INI.split('[env:'+name+']',1)[1]
assert 'extends = env:tbeam-supreme-v003-f19-dual-display-build-only' in conf
assert '-D THESIS_F22R2_PASSIVE_CAPABILITY_OBSERVER=1' in conf
for old in ('tbeam-supreme-v003-f19-dual-display-build-only','tbeam-supreme-v003-f19-dual-display-sd-pilot-v003-only'):
    assert INI.count('[env:'+old+']')==1
assert shutil.which('g++'), 'g++ needed for policy probe test'
PROGRAM=r"""
#include <cassert>
#include <initializer_list>
#include "f22_universal_capability_policy.h"
using namespace F22UniversalCapability;
int main(){
    for(unsigned id : {1U,2U,3U,4U,9999U}){
      Evidence e{};
      e.vesselId=id;e.provisionedIdentity=true;
      e.sensorWiring=SensorWiring::Shared2148;
      e.bme680Responded=e.dht20Responded=true;
      const Decision d=evaluate(e);
      assert(d.identityAccepted);
      assert(!d.sensorBusEligible && !d.weatherInputSetComplete);
      assert(!d.sdMountEligible && !d.physicalOutputsEnabled && !d.autoEnrolled);
      e.sensorWiring=SensorWiring::LegacySplit;
      const Decision legacy=evaluate(e);
      assert(legacy.identityAccepted && !legacy.sensorBusEligible && !legacy.sdMountEligible);
      e.provisionedIdentity=false;
      assert(!evaluate(e).identityAccepted);
    }
    Evidence invalid{}; invalid.vesselId=10000;invalid.provisionedIdentity=true;
    assert(!evaluate(invalid).identityAccepted);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix='f22r2_passive_') as td:
    cpp=Path(td)/'probe.cpp'; cpp.write_text(PROGRAM,encoding='utf-8')
    binary=Path(td)/('probe.exe' if sys.platform.startswith('win') else 'probe')
    build=subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(POLICY.parent),str(cpp),'-o',str(binary)],capture_output=True,text=True,timeout=90)
    assert build.returncode==0,build.stdout+build.stderr
    test=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
    assert test.returncode==0,test.stdout+test.stderr
print('PASS F22R2 passive observer source contract + C++ fail-closed identity/wiring tests')
