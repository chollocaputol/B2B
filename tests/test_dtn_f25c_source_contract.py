from pathlib import Path
import configparser
root=Path(__file__).resolve().parents[1]
def read(p):return (root/p).read_text(encoding='utf-8')
m=read('firmware/shared/storage/dtn_f24m_sos_packet_store.h')
n=read('firmware/shared/storage/dtn_f24n_sos_restart_recovery.h')
a=read('firmware/shared/storage/dtn_f25a_runtime_deny.h')
b=read('firmware/shared/storage/dtn_f25b_session_lifecycle.h')
l=read('firmware/universal/src/lora_manager.cpp')
main=read('firmware/universal/src/main.cpp')
logger=read('firmware/shared/storage/thesis_sd_logger.h')
proto=read('firmware/shared/communication/packet_protocol.h')
cfg=configparser.ConfigParser(interpolation=None);cfg.read_string((read('firmware/universal/platformio.ini')+'\n'+read('tests/fixtures/dtn/sf7_intermediate_profiles.ini')))
p=cfg['env:tbeam-supreme-f25c-control-journal-deny-build-only']
local=l[l.index('void cancelActiveSOS() {'):l.index('void processCancelRetries() {')]
rx=l[l.index('  if (p.packetType == PACKET_SOS_CANCEL) {'):l.index('  if (p.packetType == PACKET_SOS) {',l.index('  if (p.packetType == PACKET_SOS_CANCEL) {'))]
checks=[]
def check(name,ok):
 assert ok,name
 checks.append(name)
for label,ok in [
 ('inherits F25B',p['extends']=='env:tbeam-supreme-f25b-session-lifecycle-deny-build-only'),
 ('one flag',p['build_flags'].splitlines()[1:]==['${env:tbeam-supreme-f25b-session-lifecycle-deny-build-only.build_flags}','-D THESIS_DTN_F25C_BUILD_ONLY=1']),
 ('target inherited deny required','F25C target requires inherited permanent denial' in m),
 ('default off','#define THESIS_DTN_F25C_BUILD_ONLY 0' in m),
 ('wire unchanged','sizeof(VesselPacket) == 62' in proto),
 ('journal frame size unchanged','kFrameBytes = 88' in m),
 ('receipt frame size unchanged','kReceiptBytes=40' in n),
 ('SOS v1 canonical','frame[4]=1;frame[5]=item.source;' in m and 'frame[4]==1 && frame[6]==0 && p.packetType==PACKET_SOS' in m),
 ('CANCEL v2 canonical','frame[4]=2;frame[6]=PACKET_SOS_CANCEL' in m and 'frame[4]==2 && frame[6]==PACKET_SOS_CANCEL && p.packetType==PACKET_SOS_CANCEL' in m),
 ('CANCEL payload binds event','rd32(reinterpret_cast<const uint8_t*>(p.message))==p.sequenceNumber' in m),
 ('CANCEL payload binds origin','rd32(reinterpret_cast<const uint8_t*>(p.message)+4)==p.originNodeId' in m),
 ('queue typed identity','recordType(previous)==p.packetType' in m),
 ('queue exact bytes','memcmp(previous.bytes,&p,kWireBytes)==0' in m),
 ('ledger typed identity','DTNF24M::recordType(prior)==DTNF24M::recordType(p)' in n),
 ('receipt matches type','receiptType(r)==DTNF24M::recordType(p)' in n),
 ('receipt canonical v2','out[4]=2;out[6]=PACKET_SOS_CANCEL' in n and 'in[4]==2 && in[6]==PACKET_SOS_CANCEL' in n),
 ('receipt writer typed','matchesRecord(receipt,row->packet)' in n),
 ('receipt verifier required','!verifier(receipt,match->packet)' in n and '!verify(r,e.packet)' in n),
 ('receipt post-write fault','if(applied!=ReceiptResult::Verified){fault_=true;return false;}' in n),
 ('record set cancellation dominant','if(DTNF24M::recordType(p)==PACKET_SOS_CANCEL)return EventState::CancelSeen;' in n),
 ('pending pair SOS first','!rows_[j].delivered && q.origin==p.origin && q.event==p.event && DTNF24M::recordType(q)==PACKET_SOS' in n),
 ('local capture after persisted intent and construction',local.index('F05Recovery::save(0, activeEmergencyId')<local.index('memcpy(pendingCancel.message + 4')<local.index('thesisF25CObserveCancelPacket(pendingCancel,0U)')<local.index('nextCancelTxAt = millis()',local.index('thesisF25CObserveCancelPacket(pendingCancel,0U)'))),
 ('only newly accepted RX cancellation',rx.index('cancelOrigin!=p.originNodeId')<rx.index('peerSOSTable.cancel(')<rx.index('if(result==MultiPeerSOS::Result::Cancelled)(void)thesisF25CObserveCancelPacket(p,1U)')<rx.index('VesselPacket ack={}')),
 ('no retry or ACK capture',l.count('thesisF25CObserveCancelPacket(')==3),
 ('bridge type restricted','packet.packetType==PACKET_SOS_CANCEL && thesisF24MObserveSOSPacket(packet,source)' in main),
 ('single queue','static DTNF24M::PendingQueue<8> f24mSosCapture;' in logger and 'thesisF25ACapture(ThesisSDLogger::f24mSosCapture,packet,source)' in main),
 ('strict runtime retained','const DTNF25B::DenyWitness evidenceWitness_{};' in a and 'service_.append(queue)' in a and 'service_.start(' not in a),
 ('same owner/service','DTNF24Z::Service<CheckedBackend<Backend,Witness>,MountWitness,Witness> service_' in b),
 ('no fabricated closure','bool closedCount(size_t&)const{return false;}' in b),
 ('no on-air protocol modification','PACKET_SOS_CANCEL = 4' in proto and 'PACKET_SOS_CANCEL_ACK = 5' in proto),
]:check(label,ok)
print(f'F25C control journal source contract PASS: {len(checks)} checks; scoped static evidence only')
