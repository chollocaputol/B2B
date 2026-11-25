from pathlib import Path
import configparser,re
root=Path(__file__).resolve().parents[1]
read=lambda p:(root/p).read_text(encoding='utf-8')
m=read('firmware/universal/src/main.cpp'); l=read('firmware/shared/storage/thesis_sd_logger.h'); r=read('firmware/universal/src/lora_manager.cpp'); b=read('firmware/universal/src/dtn_f24y_target_backend.cpp'); h=read('firmware/shared/storage/dtn_f25a_runtime_deny.h'); ini=(read('firmware/universal/platformio.ini')+'\n'+read('tests/fixtures/dtn/sf7_intermediate_profiles.ini'))
checks=0
def check(label,ok):
 global checks
 assert ok,label
 checks+=1
cfg=configparser.ConfigParser(interpolation=None);cfg.read_string(ini)
profile='env:tbeam-supreme-f25a-runtime-deny-build-only'
check('inherits F24Z',cfg[profile]['extends']=='env:tbeam-supreme-f24z-session-service-deny-build-only')
def flags(section):
 return re.sub(r'\$\{([^}]+)\.build_flags\}',lambda x:flags(x[1]),cfg[section]['build_flags'])
effective={}
for name,value,undef in re.findall(r'-D\s+(\w+)=([01])\b|-U(\w+)',flags(profile)):
 if undef:effective.pop(undef,None)
 else:effective[name]=int(value)
for f in ['THESIS_DTN_F25A_BUILD_ONLY','THESIS_DTN_F24Z_BUILD_ONLY','THESIS_DTN_F24M_BUILD_ONLY','THESIS_DTN_F24K_SD_INTERPOSE_BUILD_ONLY','THESIS_SD_LOGGER']:
 check('required '+f,effective.get(f)==1)
for f in ['THESIS_SD_PROBE','THESIS_F23D_SD_ROLE','THESIS_F23D_CANDIDATE','THESIS_F23D_PHYSICAL_CANDIDATE','THESIS_MULTIHOP_DEV_TEST','THESIS_MULTIHOP_BENCH_RF','THESIS_LAB_RELAY_TX','THESIS_FLEET_USB_ENROLLMENT','THESIS_F22_FLEET_OPERATIONAL','THESIS_V003_F15_SD_PILOT_ARMED','THESIS_V003_F13_LIVE_OUTPUTS']:
 check('off '+f,not effective.get(f,0))
check('one preexisting queue',l.count('static DTNF24M::PendingQueue<8> f24mSosCapture;')==1 and 'Queue queue_' not in h)
check('real capture bridge','thesisF25ACapture(ThesisSDLogger::f24mSosCapture,packet,source)' in m)
check('local and received hooks intact',r.count('thesisF24MObserveSOSPacket(')==3 and 'thesisF24MObserveSOSPacket(pendingSOS,0U)' in r and 'thesisF24MObserveSOSPacket(p,1U)' in r)
check('bind from setup',m.index('void setup() {')<m.index('const bool f25aBound=thesisF25ABindLoop()')<m.index('  setupLoRa();'))
check('runtime after radio retry',m.index('  pollLoRaReceive();')<m.index('  processSOSRetries();')<m.index('  processCancelRetries();')<m.index('  thesisF25APoll(ThesisSDLogger::f24mSosCapture,thesisF25ARadioBusy());'))
check('actual target instance','DTNF25A::Runtime<Target> f25aRuntime;' in b)
check('ISR before task token',b.index('if(xPortInIsrContext())return 0;')<b.index('xTaskGetCurrentTaskHandle()'))
check('private real service','DTNF24Z::Service<Backend,DTNF24W::DenyWitness,DTNF24O::DenyWitness> service_' in h)
check('permanent witness members','const DTNF24W::DenyWitness mountWitness_{};' in h and 'const DTNF24O::DenyWitness evidenceWitness_{};' in h)
check('no fabricated session or receipt',all(x not in h for x in ['service_.start(','service_.receipt(','service_.discover(','service_.shutdown(','acknowledgeDurableAppend(']))
check('append is real','const auto result=service_.append(queue);' in h)
check('context checked for both capture/poll',h.count('if(!owns(token))return')==2 and h.count('Entry guard(entry_);if(!guard.ok)return')==2)
check('bind once','compare_exchange_strong(none,token' in h)
check('rate limit after busy',h.index('if(radioBusy)return Tick::Busy;')<h.index('uint32_t(now-last_)<30000U')<h.index('service_.append(queue)'))
check('queue retained','pending==report.pending && lost==report.lost' in h)
check('unexpected result stops further admission','halted_=true;return Tick::Unexpected;' in h)
check('radio busy snapshot','return activeEmergency || sosPending || cancelPending || macCadScanActive ||\n         getActiveSOSCount()>0 || digitalRead(LORA_DIO1)==HIGH;' in r)
check('legacy init bypass','LEGACY_SD_INIT_BYPASSED; NO_SESSION_AUTHORITY' in m and '#else\n#if THESIS_SD_LOGGER == 1\n#if THESIS_V003_F15_MACRO_SD_SANDBOX == 1' in m)
check('legacy poll bypass','void loop() {\n#if THESIS_SD_LOGGER == 1 && THESIS_DTN_F25A_BUILD_ONLY != 1' in m)
check('history bypass','#if THESIS_F06_SD_SANDBOX == 1 && THESIS_DTN_F25A_BUILD_ONLY != 1' in m)
check('export bypass','#if THESIS_DTN_F25A_BUILD_ONLY != 1\n  ThesisSDLogger::pollExport();\n#endif' in m)
for sig in ['inline void pollF24MOne(){','inline void pollF24NOne(){','inline bool f24nDiscoverClosedSessions(){','inline bool begin(XPowersAXP2101& pmu, uint32_t id) {','inline void requestExport() {','inline void pollExport() {','inline void poll() {','inline void listHistory() {','inline void requestHistory(const char* selector) {','inline void pollHistory() {','inline void pollEvents() {','inline bool appendRecord(const char* path, const char* record) {']:
 body=l.split(sig+'\n',1)[1].split('\n}',1)[0]
 deny=body.split('\n#else',1)[0]
 check('legacy body excluded '+sig,deny.startswith('#if THESIS_DTN_F25A_BUILD_ONLY == 1\n') and 'return' in deny and body.endswith('#endif // F25A legacy path excluded'))
check('no legacy SPI instance','#if THESIS_DTN_F25A_BUILD_ONLY != 1\nstatic SPIClass sdSPI(HSPI);\n#endif' in l)
check('legacy writer excluded','#if THESIS_DTN_F25A_BUILD_ONLY != 1\nstatic DTNF24M::SessionWriter f24mSosJournal;' in l)
check('legacy replay excluded','#if THESIS_DTN_F25A_BUILD_ONLY != 1\nstatic DTNF24N::Replay<32,12> f24nClosedReplay;' in l)
check('diagnostic is explicitly non-durable','durable=NO physical=DENIED' in b)
print(f'F25A runtime source contract PASS: {checks} checks; scoped static evidence only')
