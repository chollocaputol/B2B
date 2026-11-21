#include "../firmware/shared/relay/relay_dryrun_scheduler.h"
#include "../firmware/shared/relay/multihop_supervised_bench.h"
#include <cassert>
#include <cstdio>
struct Packet {
 uint16_t magic=0x4D45;
 uint8_t version=1,packetType=2;
 uint32_t originNodeId=1,sequenceNumber=10;
 uint8_t hopCount=0;
 uint8_t message[16]{};
};
int main(){
 Relay52::DryRunScheduler<Packet> q;
 Packet p{},out{};
 assert(q.offer(p,2,true)==Relay52::Admit::Queued);
 assert(q.size()==1U);
 assert(!q.peekReady(100,out)&&q.size()==1U);
 assert(!q.peekReady(349,out));
 assert(q.peekReady(350,out));
 assert(out.originNodeId==1&&out.hopCount==1);
 MultihopBench::Session session;
 assert(session.press(2,true,false,101)); // PREP
 assert(session.press(2,true,false,102)); // LIVE
 assert(!session.reserve(350,2,true,false,true)); // radio busy
 assert(q.size()==1U);
 assert(q.peekReady(351,out)&&out.sequenceNumber==10);
 assert(session.reserve(351,2,true,false,false));
 assert(q.acknowledgePrepared());
 assert(q.size()==0U);
 assert(!q.acknowledgePrepared());
 p.sequenceNumber=11;
 assert(q.offer(p,2,true)==Relay52::Admit::Queued);
 assert(!q.poll(UINT32_MAX-100,out));
 assert(q.poll(149,out));
 assert(out.sequenceNumber==11&&q.size()==0U);
 std::puts("SOS scheduler busy defer/ack retention: PASS");
}
