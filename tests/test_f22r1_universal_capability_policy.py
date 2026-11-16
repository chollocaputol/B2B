"""F22R1: pure C++ policy regression matrix. No device, Git, or storage writes."""
from pathlib import Path
import subprocess
import tempfile
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'firmware/shared/hardware/f22_universal_capability_policy.h'
assert HEADER.is_file(), 'F22R1 policy header is missing'
s = HEADER.read_text('utf-8')
assert 'namespace F22UniversalCapability' in s
assert 'd.physicalOutputsEnabled = false;' in s
assert 'd.autoEnrolled = false;' in s
for forbidden in ('digitalWrite(', 'pinMode(', 'SD.begin(', 'Preferences', '#include <Arduino.h>', 'radio.transmit('):
    assert forbidden not in s, f'Unexpected hardware side effect: {forbidden}'
assert shutil.which('g++'), 'g++ required for the F22R1 host policy matrix'
program = r'''
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include "f22_universal_capability_policy.h"
using namespace F22UniversalCapability;
int main() {
  for (uint32_t id : {1U,2U,3U,4U,9999U}) {
    Evidence e;
    e.vesselId=id; e.provisionedIdentity=true; e.boardRevisionReviewed=true;
    e.sensorPinsReviewed=true; e.sensorWiring=SensorWiring::Shared2148;
    e.bme680Responded=true; e.dht20Responded=true;
    e.sdCardPresent=true; e.sdMediaReviewed=true; e.sdExplicitOptIn=true;
    Decision d=evaluate(e);
    assert(d.identityAccepted && d.sensorBusEligible && d.weatherInputSetComplete && d.sdMountEligible);
    assert(!d.gpio38and39ReservedForDht && !d.physicalOutputsEnabled && !d.autoEnrolled);
    e.sensorWiring=SensorWiring::LegacySplit;
    d=evaluate(e);
    assert(d.sensorBusEligible && d.gpio38and39ReservedForDht && !d.physicalOutputsEnabled);
    e.sensorWiring=SensorWiring::Unverified;
    d=evaluate(e);
    assert(!d.sensorBusEligible && !d.weatherInputSetComplete && d.sdMountEligible);
    e.sensorWiring=SensorWiring::Shared2148;
    e.sensorPinsReviewed=false;
    d=evaluate(e);
    assert(!d.sensorBusEligible && !d.weatherInputSetComplete);
    e.sensorPinsReviewed=true;
    e.bme680Responded=false;
    d=evaluate(e);
    assert(d.sensorBusEligible && !d.weatherInputSetComplete);
    e.bme680Responded=true; e.dht20Responded=false;
    d=evaluate(e);
    assert(d.sensorBusEligible && !d.weatherInputSetComplete);
    e.dht20Responded=true;
    e.sdExplicitOptIn=false;
    assert(!evaluate(e).sdMountEligible);
    e.sdExplicitOptIn=true; e.sdMediaReviewed=false;
    assert(!evaluate(e).sdMountEligible);
    e.sdMediaReviewed=true; e.sdCardPresent=false;
    assert(!evaluate(e).sdMountEligible);
    e.sdCardPresent=true; e.boardRevisionReviewed=false;
    d=evaluate(e);
    assert(d.identityAccepted && !d.sensorBusEligible && !d.weatherInputSetComplete && !d.sdMountEligible);
    e.boardRevisionReviewed=true; e.provisionedIdentity=false;
    d=evaluate(e);
    assert(!d.identityAccepted && !d.sensorBusEligible && !d.sdMountEligible && !d.physicalOutputsEnabled);
  }
  for(uint32_t bad : {0U,10000U,4294967295U}) {
    Evidence e; e.vesselId=bad; e.provisionedIdentity=true;
    e.boardRevisionReviewed=true; e.sensorPinsReviewed=true; e.sensorWiring=SensorWiring::Shared2148;
    e.bme680Responded=true; e.dht20Responded=true;
    e.sdCardPresent=true; e.sdMediaReviewed=true; e.sdExplicitOptIn=true;
    Decision d=evaluate(e);
    assert(!d.identityAccepted && !d.sensorBusEligible && !d.weatherInputSetComplete && !d.sdMountEligible);
  }
  assert(pins(SensorWiring::Unverified).dhtSda==0);
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='f22r1_host_') as d:
    source = Path(d) / 'policy_test.cpp'
    exe = Path(d) / ('policy_test.exe' if sys.platform == 'win32' else 'policy_test')
    source.write_text(program, encoding='utf-8')
    compile_run = subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror', '-I',str(HEADER.parent),str(source),'-o',str(exe)], capture_output=True, text=True, timeout=90)
    if compile_run.returncode:
        print(compile_run.stdout, compile_run.stderr, file=sys.stderr)
        raise AssertionError('F22R1 policy C++ compilation failed')
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    if run.returncode:
        print(run.stdout, run.stderr, file=sys.stderr)
        raise AssertionError('F22R1 policy cases failed')
print('PASS F22R1 C++ policy matrix: identity 1,2,3,4,9999; missing/bad ID; both sensor wirings; absent sensors; SD opt-in; outputs off; no auto-enrollment')
