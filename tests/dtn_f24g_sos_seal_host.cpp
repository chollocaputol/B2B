// F24G host-only synthetic closed-SOS seal and failure-path regression.
// Never accesses a physical SD, Wi-Fi, LoRa, database, or original V002 file.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "dtn_f24g_sos_seal.h"

using namespace DTNF24G;

static uint32_t tests=0U;
#define CHECK(expr) do { \
  if (!(expr)) {std::fprintf(stderr,"F24G FAIL line %d: %s\n", __LINE__,#expr);return 1;} \
  ++tests; \
} while(false)

static void digest(const std::string& bytes,uint8_t out[32]) {
  DTNSha256::Hasher sha;
  assert(sha.update(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()));
  assert(sha.end(out));
}
static Stream makeStream(const std::string& file) {
  Stream s{};
  s.uploader.bytes[0]=0x19U;
  s.uploader.bytes[6]=0x40U; // UUID v4 format only, NOT authentication
  s.uploader.bytes[8]=0x80U;
  s.vessel=2U;
  std::memcpy(s.session,"S58BAFE2",9U);
  s.journal=DTNAckLedger::Journal::SosEvents;
  digest(file,s.sha256);
  return s;
}
static CloseFacts makeFacts(const Stream& s,uint64_t bytes) {
  CloseFacts c{};
  c.stream=s;c.fileBytes=bytes;
  c.mediaTag=0xA0B0U;c.mountEpoch=9U;c.sourceEpoch=34U;
  c.closeSerial=7U;c.ownerLeaseSerial=105U;
  c.retiredSession=true;c.appendQueueDrained=true;c.flushSucceeded=true;
  c.allWriterHandlesClosed=true;c.finalNewlineVerified=true;
  c.protectedAgainstWrites=true;
  return c;
}
static DTNF24F::OwnerNow makeOwner(const Stream& s,const CloseFacts& c) {
  DTNF24F::OwnerNow n{};
  n.uploader=s.uploader;n.vessel=s.vessel;n.mediaTag=c.mediaTag;
  n.mountEpoch=c.mountEpoch;n.sourceEpoch=c.sourceEpoch;
  n.exclusiveLeaseSerial=c.ownerLeaseSerial;
  n.mounted=true;n.mediaIdentityVerified=true;n.soleMainLoopOwner=true;
  n.sourceReadProtected=true;n.writerActive=false;n.pendingAppends=false;
  n.currentSession=false;n.urgentOrRadio=false;
  return n;
}
// Actual F24D cooperative hash against synthetic in-RAM bytes.
struct MockFile {
  const std::string* content=nullptr;
  bool isOpen()const{return content!=nullptr;}
  uint64_t size()const{return content?content->size():0U;}
  bool readBlockAt(uint64_t at,uint8_t* out,size_t length)const {
    if(!content||!out||at>content->size()||length>content->size()-at)return false;
    std::memcpy(out,content->data()+at,length);return true;
  }
  void close(){content=nullptr;}
};
struct MockVolume {
  const std::string* content=nullptr;
  unsigned opens=0U;
  bool openReadOnly(const char* path,MockFile& out) {
    if(!path||!std::strstr(path,"/sos_events.jsonl")||!content)return false;
    ++opens;out.content=content;return true;
  }
};
struct HashGate {
  bool granted=true;
  bool allowClosedJournal(const Stream& s,const char* p)const {
    char expected[96]{};
    return granted && DTNF24F::sosPath(s,expected,sizeof(expected)) &&
           p && std::strcmp(p,expected)==0;
  }
};
static DTNF24D::Result cooperativeHash(const Stream& stream,const std::string& bytes) {
  MockVolume volume{&bytes,0U};
  HashGate gate{};
  DTNF24D::Job<MockFile,MockVolume> job;
  auto r=job.begin(volume,stream,gate);
  for(unsigned i=0U;i<10000U && (r==DTNF24D::Result::Running ||
                                        r==DTNF24D::Result::Paused);++i)
    r=job.step(gate,false,128U);
  return r;
}
struct FakeSealPort {
  bool exists=false;
  bool unreadable=false;
  bool writeError=false;
  bool simulateTorn=false;
  bool* revokeDuringRead=nullptr;
  unsigned reads=0U,writes=0U;
  std::string lastPath{};
  std::vector<uint8_t> stored{};
  bool read(const char* path,uint8_t* out,size_t cap,size_t& length,bool& found) {
    ++reads;
    lastPath=path?path:"";
    if(revokeDuringRead)*revokeDuringRead=false;
    if(unreadable)return false;
    found=exists;
    length=exists?stored.size():0U;
    if(exists && length<=cap && out)std::copy(stored.begin(),stored.end(),out);
    return true;
  }
  bool writeOnce(const char* path,const uint8_t* bytes,size_t n) {
    ++writes;
    lastPath=path?path:"";
    if(writeError||exists)return false;
    exists=true;
    const size_t actual=simulateTorn?n/2U:n;
    stored.assign(bytes,bytes+actual);
    return !writeError;
  }
};
struct MutableGate {
  mutable bool allowed=true;
  mutable bool disableAfterRead=false;
  bool permitSealSidecar(const Stream&)const {
    if(!allowed)return false;
    if(disableAfterRead){disableAfterRead=false;allowed=false;}
    return true;
  }
};
static_assert(kSealBytes==106U,"F24G wire length changed");

int main() {
  const std::string original="{\"kind\":\"sos_local_start\",\"vessel_id\":2,\"event_index\":1}\n";
  const Stream stream=makeStream(original);
  CloseFacts facts=makeFacts(stream,original.size());
  DTNF24F::OwnerNow owner=makeOwner(stream,facts);
  Seal seal{};

  // Start denied. No seal can be produced from guessed flags.
  CloseFacts none{};DTNF24F::OwnerNow nobody{};
  CHECK(prepare(stream,none,nobody,DTNF24D::Result::Verified,original.size(),seal)
       ==Result::BadClose);
  CHECK(!valid(seal));
  CHECK(cooperativeHash(stream,original)==DTNF24D::Result::Verified);
  CHECK(prepare(stream,facts,owner,DTNF24D::Result::Verified,original.size(),seal)
       ==Result::ReadyCandidate);
  CHECK(valid(seal));
  CHECK(seal.fileBytes==original.size());

  // SOS journal only; a normal/telemetry stream is never operational SOS.
  Stream telemetry=stream;telemetry.journal=DTNAckLedger::Journal::Telemetry;
  Seal rejected{};
  CHECK(prepare(telemetry,facts,owner,DTNF24D::Result::Verified,original.size(),rejected)
       ==Result::BadStream);
  CHECK(!sealPath(telemetry,nullptr,0U));

  // Every close barrier must be affirmatively satisfied.
  CloseFacts c=facts;c.retiredSession=false;
  CHECK(prepare(stream,c,owner,DTNF24D::Result::Verified,c.fileBytes,rejected)==Result::BadClose);
  c=facts;c.appendQueueDrained=false;
  CHECK(prepare(stream,c,owner,DTNF24D::Result::Verified,c.fileBytes,rejected)==Result::BadClose);
  c=facts;c.flushSucceeded=false;
  CHECK(prepare(stream,c,owner,DTNF24D::Result::Verified,c.fileBytes,rejected)==Result::BadClose);
  c=facts;c.allWriterHandlesClosed=false;
  CHECK(prepare(stream,c,owner,DTNF24D::Result::Verified,c.fileBytes,rejected)==Result::BadClose);
  c=facts;c.finalNewlineVerified=false;
  CHECK(prepare(stream,c,owner,DTNF24D::Result::Verified,c.fileBytes,rejected)==Result::BadClose);
  c=facts;c.protectedAgainstWrites=false;
  CHECK(prepare(stream,c,owner,DTNF24D::Result::Verified,c.fileBytes,rejected)==Result::BadClose);
  c=facts;c.closeSerial=0U;
  CHECK(prepare(stream,c,owner,DTNF24D::Result::Verified,c.fileBytes,rejected)==Result::WrongOwner);

  // Existing owner and mount must be exact throughout the barrier.
  DTNF24F::OwnerNow n=owner;n.writerActive=true;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.pendingAppends=true;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.currentSession=true;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.urgentOrRadio=true;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.exclusiveLeaseSerial=106U;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.mountEpoch++;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.mediaTag++;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.sourceEpoch++;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.soleMainLoopOwner=false;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.sourceReadProtected=false;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  n=owner;n.mediaIdentityVerified=false;
  CHECK(prepare(stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes,rejected)==Result::WrongOwner);
  CHECK(prepare(stream,facts,owner,DTNF24D::Result::Running,facts.fileBytes,rejected)==Result::HashNotVerified);
  CHECK(prepare(stream,facts,owner,DTNF24D::Result::DigestMismatch,facts.fileBytes,rejected)==Result::HashNotVerified);
  CHECK(prepare(stream,facts,owner,DTNF24D::Result::Verified,facts.fileBytes+1U,rejected)==Result::ChangedSource);

  // Exact portable fixed wire / checksum, not host struct layout.
  uint8_t wire[kSealBytes]{};
  CHECK(encode(seal,wire));
  Seal restored{};
  CHECK(decode(wire,sizeof(wire),restored));
  CHECK(sameSeal(seal,restored));
  CHECK(!decode(wire,sizeof(wire)-1U,restored));
  CHECK(!valid(restored));
  wire[33U]^=0x01U;
  CHECK(!decode(wire,sizeof(wire),restored));
  wire[33U]^=0x01U;
  CHECK(decode(wire,sizeof(wire),restored));
  char sealpath[kSealPathBytes]{};
  CHECK(sealPath(stream,sealpath,sizeof(sealpath)));
  CHECK(std::string(sealpath)=="/THESIS/DTNSYNC/V002/S58BAFE2/sos.close");
  CHECK(std::string(sealpath).find("F06TEST")==std::string::npos);

  // No implicit physical permission even after a valid candidate.
  NoPhysicalSealPermission deny{};
  FakeSealPort port{};
  Store<FakeSealPort,NoPhysicalSealPermission> deniedStore(port,deny);
  CHECK(deniedStore.publishOnce(seal)==Publish::Denied);
  CHECK(port.reads==0U && port.writes==0U);
  CHECK(deniedStore.load(stream,restored)==Load::Denied);
  CHECK(port.reads==0U && port.writes==0U);
  CHECK(!DTNF24F::SosClosedLease{}.allowClosedJournal(stream,""));

  // Mock sidecar only: first write, readback and identical retry.
  MutableGate allow{};
  Store<FakeSealPort,MutableGate> store(port,allow);
  CHECK(store.load(stream,restored)==Load::Missing);
  CHECK(store.publishOnce(seal)==Publish::WrittenAndReadBack);
  CHECK(port.writes==1U);
  CHECK(store.load(stream,restored)==Load::Ready);
  CHECK(sameSeal(seal,restored));
  CHECK(store.publishOnce(seal)==Publish::IdenticalExisting);
  CHECK(port.writes==1U);
  Seal altered=seal;altered.closeSerial++;altered.crc=checksum(altered);
  CHECK(valid(altered));
  CHECK(store.publishOnce(altered)==Publish::Conflict);
  CHECK(port.writes==1U);

  // Gate loss during an in-progress seal read: no result may be published.
  FakeSealPort interrupted=port;
  MutableGate drop{};
  interrupted.revokeDuringRead=&drop.allowed;
  Store<FakeSealPort,MutableGate> interruptedStore(interrupted,drop);
  CHECK(interruptedStore.load(stream,restored)==Load::Denied);
  CHECK(!valid(restored));
  CHECK(interrupted.writes==1U);
  CHECK(interruptedStore.publishOnce(seal)==Publish::Denied);

  // Torn existing record must not be overwritten or auto-recovered.
  FakeSealPort torn{};torn.simulateTorn=true;
  Store<FakeSealPort,MutableGate> tornStore(torn,allow);
  CHECK(tornStore.publishOnce(seal)==Publish::IoError);
  CHECK(torn.writes==1U);
  CHECK(tornStore.load(stream,restored)==Load::Corrupt);
  CHECK(tornStore.publishOnce(seal)==Publish::Conflict);
  CHECK(torn.writes==1U);
  torn.stored[0]^=0xFFU;
  CHECK(tornStore.load(stream,restored)==Load::Corrupt);
  torn.unreadable=true;
  CHECK(tornStore.load(stream,restored)==Load::IoError);

  // Rechecking the original journal catches same-size changes IF fresh
  // source bytes are actually rescanned under an unbroken owner barrier.
  CHECK(recheckAtScan(seal,stream,facts,owner,cooperativeHash(stream,original),
                     original.size())==Result::ReadyCandidate);
  std::string mutated=original;
  mutated[7U]=(mutated[7U]=='X')?'Y':'X';
  CHECK(mutated.size()==original.size());
  CHECK(cooperativeHash(stream,mutated)==DTNF24D::Result::DigestMismatch);
  CHECK(recheckAtScan(seal,stream,facts,owner,cooperativeHash(stream,mutated),
                     mutated.size())==Result::HashNotVerified);
  c=facts;c.sourceEpoch++;
  n=owner;n.sourceEpoch++;
  CHECK(recheckAtScan(seal,stream,c,n,DTNF24D::Result::Verified,c.fileBytes)
       ==Result::ChangedSource);
  c=facts;c.closeSerial++;
  CHECK(recheckAtScan(seal,stream,c,owner,DTNF24D::Result::Verified,c.fileBytes)
       ==Result::ChangedSource);
  n=owner;n.mounted=false;
  CHECK(recheckAtScan(seal,stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes)
       ==Result::WrongOwner);
  n=owner;n.mountEpoch++;
  CHECK(recheckAtScan(seal,stream,facts,n,DTNF24D::Result::Verified,facts.fileBytes)
       ==Result::WrongOwner);
  c=facts;c.fileBytes++;
  CHECK(recheckAtScan(seal,stream,c,owner,DTNF24D::Result::Verified,c.fileBytes)
       ==Result::ChangedSource);

  // A replacement media with a different tag always invalidates the seal.
  c=facts;c.mediaTag++;
  n=owner;n.mediaTag++;
  CHECK(recheckAtScan(seal,stream,c,n,DTNF24D::Result::Verified,c.fileBytes)
       ==Result::ChangedSource);

  // Unknown/mismatched streams and gate loss deny without publication.
  Stream other=stream;other.session[7]=(other.session[7]=='E')?'F':'E';
  CHECK(store.load(other,restored)==Load::WrongBinding);
  allow.allowed=false;
  const unsigned reads=port.reads,writes=port.writes;
  CHECK(store.publishOnce(seal)==Publish::Denied);
  CHECK(store.load(stream,restored)==Load::Denied);
  CHECK(port.reads==reads && port.writes==writes);
  allow.allowed=true;

  std::printf("F24G SOS close-barrier / fixed seal / torn-write / fresh digest / default-deny: PASS (%u checks)\n",tests);
  return 0;
}
