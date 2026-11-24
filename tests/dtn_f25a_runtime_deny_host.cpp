#include "../firmware/shared/storage/dtn_f25a_runtime_deny.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace DTNF25A;
static unsigned checks=0, io=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
// All backend operations count if unexpectedly reached. Actual F24Z service
// is used, not a stub returning denial.
struct Backend {
 bool mount(){++::io;return false;}
 bool unmount(){++::io;return false;}
 bool exists(const char*,bool&){++::io;return false;}
 bool open(uint8_t,const char*,DTNF24W::Mode){++::io;return false;}
 bool child(uint8_t,uint8_t){++::io;return false;}
 bool io(uint8_t,DTNF24W::Action,uint8_t*,size_t){++::io;return false;}
 bool size(uint8_t,size_t&){++::io;return false;}
 bool seek(uint8_t,size_t){++::io;return false;}
 bool flush(uint8_t){++::io;return false;}
 bool close(uint8_t){++::io;return false;}
};
static VesselPacket sos(unsigned id,unsigned seq){VesselPacket p{};p.magic=PACKET_MAGIC;p.version=PACKET_VERSION;p.packetType=PACKET_SOS;p.originNodeId=id;p.sequenceNumber=seq;return p;}
int main(){
 Runtime<Backend> runtime;Queue q;Report report{};const auto p=sos(2,1);
 CHECK(!runtime.bind(0));CHECK(runtime.capture(1,q,p,0)==DTNF24M::CaptureResult::Faulted);
 CHECK(runtime.poll(1,q,0,false,report)==Tick::ContextDenied);CHECK(runtime.bind(1));
 CHECK(!runtime.bind(2));CHECK(!runtime.bind(1));
 CHECK(runtime.poll(0,q,0,false,report)==Tick::ContextDenied);
 CHECK(runtime.poll(2,q,0,false,report)==Tick::ContextDenied);
 CHECK(runtime.capture(2,q,p,0)==DTNF24M::CaptureResult::Faulted);CHECK(q.pending()==0);
 CHECK(runtime.poll(1,q,0,true,report)==Tick::Busy);
 CHECK(runtime.poll(1,q,0,false,report)==Tick::Denied);CHECK(report.pending==0);
 CHECK(runtime.capture(1,q,p,0)==DTNF24M::CaptureResult::Queued);
 CHECK(runtime.capture(1,q,p,0)==DTNF24M::CaptureResult::Duplicate);
 CHECK(runtime.capture(1,q,p,2)==DTNF24M::CaptureResult::Invalid);
 auto cancel=p;cancel.packetType=PACKET_SOS_CANCEL;
 CHECK(runtime.capture(1,q,cancel,0)==DTNF24M::CaptureResult::Invalid);
 for(unsigned i=2;i<=8;++i)CHECK(runtime.capture(1,q,sos(3,i),1)==DTNF24M::CaptureResult::Queued);
 CHECK(runtime.capture(1,q,sos(3,9),1)==DTNF24M::CaptureResult::Full);
 CHECK(q.pending()==8 && q.lost()==1);CHECK(!std::memcmp(q.front()->bytes,&p,sizeof(p)));
 CHECK(runtime.poll(1,q,29999,false,report)==Tick::NotDue);
 CHECK(runtime.poll(1,q,30000,true,report)==Tick::Busy);
 CHECK(runtime.poll(1,q,30000,false,report)==Tick::Denied);CHECK(report.pending==8 && report.lost==1);
 CHECK(runtime.poll(1,q,30000,false,report)==Tick::NotDue);
 CHECK(!std::memcmp(q.front()->bytes,&p,sizeof(p)));CHECK(q.pending()==8 && q.lost()==1);
 CHECK(runtime.poll(1,q,0xfffffff0U,false,report)==Tick::Denied);
 CHECK(runtime.poll(1,q,10,false,report)==Tick::NotDue);
 CHECK(runtime.poll(1,q,30000,false,report)==Tick::Denied);
 CHECK(q.pending()==8 && q.lost()==1);CHECK(io==0);
 std::printf("F25A runtime deny host PASS: %u checks; backend_calls=%u\n",checks,io);
}
