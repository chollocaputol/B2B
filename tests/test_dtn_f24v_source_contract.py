#!/usr/bin/env python3
"""F24V: exact captured Arduino core 3.3.12 and fail-closed DTN source contract.
Runs read-only. Does not grant physical media ownership or prove card durability.
"""
import hashlib
from pathlib import Path
import sys

repo=Path(__file__).resolve().parents[1]
if len(sys.argv)!=2:
 raise SystemExit('usage: test_dtn_f24v_source_contract.py <installed-core-root>')
core=Path(sys.argv[1]).resolve()
if not core.is_dir():
 raise SystemExit(f'STOP: installed Arduino framework not found: {core}')
expected={
 'libraries/FS/src/FS.h':'B0F42CD30763DBCD70D1A465F407A8B257749459B8169F603D92DF4A8EF9100F',
 'libraries/FS/src/FS.cpp':'B031B23337D69CAC448E7E19184BE751428AEB1C282CAF3CDC8DFD604B36DAE2',
 'libraries/FS/src/FSImpl.h':'3AA7A27613D13404F557C0DC3CC3EF6188AC132B0E026835E9316B35226AFB73',
 'libraries/FS/src/vfs_api.h':'9D1C70F896BD68A7336792A8456C2EF567A66BF26C09EBDCC448380BE8DC9F7F',
 'libraries/FS/src/vfs_api.cpp':'3D345BCB3B764F08C6E923DBACC96F27DA10EDB12BEB774DE439228057D14A54',
 'libraries/SD/src/SD.h':'191D3051C5D8281986CE6A94DA9C2291AB4BA365C6D35679720E5F854DD3DE1F',
 'libraries/SD/src/SD.cpp':'9728E9FD4ABE200048291384B7FFBF12D2E8FE182F3685171442767565BC4FE0',
 'libraries/SD/src/sd_diskio.cpp':'FDA00B53F23D4D93AB63A506B3197F27E24278A3F0D5DA9C7590FFF5EC37AA05',
 'libraries/SD/src/sd_diskio_crc.c':'C0305B3157516D016D68751786CBFE318B2367B35231A1016D64D636507EFBC3',
 'package.json':'1B4D8E93A1FCD9E8935312D0D134BCA889142A92B7415B660C77EB165BDDE963',
 '.piopm':'F56E627DD0AC7200D18ABE4F5BB83D75D1ADFA753AF6A497DB274FBC9F47D557',
}
checks=[]
def test(label,condition):
 checks.append(label)
 if not condition:
  raise AssertionError('F24V SOURCE CONTRACT FAILED: '+label)
for path,digest in expected.items():
 f=core/path
 test('core existence: '+path,f.is_file())
 test('installed core SHA256: '+path,hashlib.sha256(f.read_bytes()).hexdigest().upper()==digest)
fs=(core/'libraries/FS/src/FS.h').read_text()
cpp=(core/'libraries/FS/src/FS.cpp').read_text()
vfs=(core/'libraries/FS/src/vfs_api.cpp').read_text()
sd=(core/'libraries/SD/src/SD.cpp').read_text()
test('real FileImpl is shared_ptr','typedef std::shared_ptr<FileImpl> FileImplPtr' in fs)
test('real File class contains shared impl','FileImplPtr _p;' in fs)
test('real File close calls impl close','_p->close();' in cpp)
test('real File close drops instance ref','_p = nullptr;' in cpp)
test('real VFS destructor calls close','VFSFileImpl::~VFSFileImpl()' in vfs and '  close();' in vfs.split('VFSFileImpl::~VFSFileImpl()',1)[1][:80])
test('real VFS can close stdio','fclose(_f);' in vfs)
test('real VFS can close directory','closedir(_d);' in vfs)
test('SD unmount independent','sdcard_unmount' in sd and 'sdcard_uninit' in sd)
q=(repo/'firmware/shared/storage/dtn_f24q_cross_writer_arbiter.h').read_text()
r=(repo/'firmware/shared/storage/dtn_f24r_guarded_file_port.h').read_text()
logger=(repo/'firmware/shared/storage/thesis_sd_logger.h').read_text()
ini=((repo/'firmware/universal/platformio.ini').read_text()+'\n'+(repo/'tests/fixtures/dtn/sf7_intermediate_profiles.ini').read_text())
h=(repo/'firmware/shared/storage/dtn_f24v_core_close_policy.h').read_text()
test('new lease precheck declared','bool permitClose(const Lease& l,uint8_t slot)' in q)
test('precheck does not clear slot','bool permitClose(' in q and 'handles_[slot]=false' not in q.split('bool permitClose(',1)[1].split('// Closing a File',1)[0])
test('precheck rejects stale owner','if(!check(l) || slot>=2U || !handles_[slot])return false;' in q)
test('precheck preserves child order','(active_==Lane::F23D || active_==Lane::History) && slot==0U && handles_[1]' in q)
test('guard checks before close','if(port_->permitCloseSlot(slot_))' in r)
test('guard close then release','raw_.close();\n       if(port_->closeSlot(slot_))port_=nullptr;' in r)
test('legacy branch preserved','#else\n     if(port_->closeSlot(slot_)){raw_.close();port_=nullptr;}\n#endif' in r)
test('new branch only active in F24V','defined(THESIS_DTN_F24V_BUILD_ONLY) && THESIS_DTN_F24V_BUILD_ONLY == 1' in r)
test('guard delegates preclose','return arbiter_.permitClose(lease_,slot);' in r)
test('no real sd owner flag','inline constexpr bool kPhysicalOwnerGranted=false;' in h)
test('raw File destruct unsafe flag','inline constexpr bool kRawFileDestructionCertified=false;' in h)
test('no raw flush/close durability flag','inline constexpr bool kImplicitCloseDurable=false;' in h)
test('Arduino 3.3.12 pinned',all(s in h for s in ['kAuditedArduinoCoreMajor=3','kAuditedArduinoCoreMinor=3','kAuditedArduinoCorePatch=12']))
test('F24V flag defaults off','#define THESIS_DTN_F24V_BUILD_ONLY 0' in logger)
test('F24V validates flag','THESIS_DTN_F24V_BUILD_ONLY != 0 && THESIS_DTN_F24V_BUILD_ONLY != 1' in logger)
for flag in ['THESIS_DTN_F24U_BUILD_ONLY','THESIS_DTN_F24T_BUILD_ONLY','THESIS_DTN_F24S_BUILD_ONLY','THESIS_DTN_F24R_BUILD_ONLY','THESIS_DTN_F24Q_BUILD_ONLY','THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY']:
 test('F24V requires '+flag,flag+' != 1' in logger)
for flag in ['THESIS_F23D_SD_EVIDENCE','THESIS_F22_FLEET_OPERATIONAL','THESIS_LAB_RELAY_TX','THESIS_V003_F13_LIVE_OUTPUTS']:
 test('F24V requires '+flag+' OFF',flag+' != 0' in logger)
test('F24V requires F06 sandbox','THESIS_F06_SD_SANDBOX != 1' in logger)
test('deny witness remains','DTNF24Q::DenyWitness' in logger and 'f24rNoLease{}' in logger)
test('policy header included','#include "dtn_f24v_core_close_policy.h"' in logger)
test('profile extends F24U','extends = env:tbeam-supreme-f24u-teardown-deny-build-only' in ini.split('[env:tbeam-supreme-f24v-core-close-deny-build-only]')[1])
test('profile flag present','-D THESIS_DTN_F24V_BUILD_ONLY=1' in ini)
test('no upload in F24V profile','upload_protocol' not in ini.split('[env:tbeam-supreme-f24v-core-close-deny-build-only]')[-1])
test('no SD calls in F24V policy header','SD.open' not in h and '#include <SD.h>' not in h)
test('no RF calls in F24V policy header','RadioLib' not in h and 'transmit' not in h)
print(f'F24V installed Arduino-ESP32 3.3.12 core-bound DTN deny contract PASS: {len(checks)} checks; physical implicit teardown UNSAFE')
