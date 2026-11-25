from pathlib import Path
import configparser
r=Path(__file__).resolve().parents[1]
read=lambda x:(r/x).read_text(encoding='utf-8')
m=read('firmware/universal/src/main.cpp');t=read('firmware/universal/src/study_sf_target.inc');b=read('firmware/universal/src/dtn_f24y_target_backend.cpp');j=read('firmware/shared/storage/study_sf_journal.h');l=read('firmware/universal/src/lora_manager.cpp')
cfg=configparser.ConfigParser(interpolation=None);cfg.read_string(read('firmware/universal/platformio.ini'));env=cfg['env:tbeam-supreme-study-sf-build-only'];checks=[
 env['extends']=='env:tbeam-supreme-fleet-universal-operational-build-only',
 '-D THESIS_STUDY_SF_BUILD_ONLY=1' in env['build_flags'],
 'const StudyConfig studyConfig{false,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr}' in t,
 'if(!thesisStudySfConfigured()||studyQueue' in t,
 'if(!config||!storageUsable(*config))' in t,
 'return usable(c);' in t and '#if THESIS_STUDY_SF_OFFLINE_V003 == 1\n return true;' in t,
 'thesisStudySfConfigured() && VesselIdentity::ready()' in m,
 'static EspMount mount;static StudySF::Journal journal;' in t,
 'kSdHost=SPI3_HOST' in b,
 'thesisF25APoll(ThesisSDLogger::f24mSosCapture' in m and 'THESIS_DTN_F25A_BUILD_ONLY == 1 && THESIS_STUDY_SF_BUILD_ONLY != 1' in m,
 'thesisStudySfControl(millis(),studyUtc(utc),packet,source)' in m,
 'thesisF24MObserveSOSPacket(pendingSOS,0U)' in l and 'thesisF25CObserveCancelPacket(pendingCancel,0U)' in l,
 'studyMacroCapture();' in m and 'V003F10Micro::stateName(f10Forecaster.state())' in m,
 'thesisStudySfGps(studyLastGps' in m,
 'xQueueSend(studyQueue,&r,0)' in t and 'xQueueCreate(8,sizeof(StudySF::Record))' in t,
 'cfg.cert_pem=c.ca' in t and 'cfg.disable_auto_redirect=true' in t and 'HTTP_TRANSPORT_OVER_SSL' in t,
 'cfg.timeout_ms=5000' in t and 'sizeof(b->body)-b->len' in t,
 'WiFi.persistent(false)' in t and '30000U' in t,
 'O_WRONLY|O_CREAT|O_EXCL' in j and 'fsync(fileno(f))' in j,
 'if(!confirmed(pending_,node_,status,body))return false;offset_=pending_.end' in j,
 'MaxBatch=8' in j and 'MaxBody=8192' in j,
 'Torn' in j and 'Torn' in t,
 all(x not in j for x in ['remove(', 'unlink(', 'rename(', 'truncate(']),
 'thesis_study_sf_link_anchor' in t,
 'setInsecure' not in t and 'service_role' not in t,
 '-D THESIS_STUDY_SF_OFFLINE_V003=1' in cfg['env:tbeam-supreme-study-sf-v003-offline-bench']['build_flags'],
 '-D THESIS_STUDY_SF_SYNC_V003=1' in cfg['env:tbeam-supreme-study-sf-v003-sync-bench']['build_flags'],
 '#include "study_sf_local_config.generated.h"' in t and 'if(node!=3U)return false;' in t,
 'std::atomic<bool> studyContinuous{false};' in t,
 all('if(!thesisStudySfIsContinuous())return false;' in t.split('bool thesisStudySf'+name+'(',1)[1].split('}',1)[0] for name in ('Weather','Gps')),
 'thesisStudySfIsContinuous' not in t.split('bool thesisStudySfControl(',1)[1],
 'cfg.buffer_size_tx=4096' in t,
 'thesisStudySfSetContinuous(sdHybridMenuCursor == 1)' in m,
 ]
for i,c in enumerate(checks,1): assert c,f'Study SF source check {i}'
print(f'STUDY SF firmware source PASS: {len(checks)} checks; source contracts only, not deployment authorization')
