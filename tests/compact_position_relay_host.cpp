#include "../firmware/shared/relay/compact_position_relay.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace CompactPositionRelay;

static void make(uint8_t (&b)[25], uint32_t origin=1,
                 uint32_t sequence=1, uint32_t uptime=42) {
  CompactPositionV2::State p{};
  p.originId=origin; p.sequence=sequence; p.uptimeSeconds=uptime;
  p.gpsValid=true; p.latitudeE7=82500000; p.longitudeE7=1242500000;
  assert(CompactPositionV2::encode(b,sizeof(b),p));
}
int main() {
  Queue q;
  Frame f{};
  uint8_t b[25]{};
  make(b);
  assert(q.offer(b,sizeof(b),2)==Admission::Queued);
  assert(q.offer(b,sizeof(b),2)==Admission::Duplicate);
  assert(q.pending()==1);
  assert(!q.take(f,true)); // emergency first
  assert(q.pending()==1);
  assert(q.peek(f,false)); // queue head is not consumed by a radio-busy defer
  assert(q.pending()==1);
  assert(q.take(f,false));
  assert(q.pending()==0);
  assert(!memcmp(f.bytes,b,25)); // immutable on-air bytes
  CompactPositionV2::State decoded;
  assert(CompactPositionV2::decode(f.bytes,25,decoded));
  assert(decoded.originId==1 && decoded.sequence==1);
  assert(decoded.latitudeE7==82500000);
  assert(q.offer(b,sizeof(b),3)==Admission::WrongRelay);
  assert(q.offer(b,24,2)==Admission::Invalid);
  b[2]=8;
  assert(q.offer(b,sizeof(b),2)==Admission::Invalid);
  make(b,2,10);
  assert(q.offer(b,sizeof(b),2)==Admission::OwnOrigin);
  make(b,3,11);
  assert(q.offer(b,sizeof(b),2)==Admission::WrongOrigin);
  for (uint32_t seq=20;seq<=23;++seq) {
    make(b,1,seq);
    assert(q.offer(b,sizeof(b),2)==Admission::Queued);
  }
  make(b,1,24);
  assert(q.offer(b,sizeof(b),2)==Admission::Full);
  assert(q.take(f,false) && f.sequence==20);
  assert(q.offer(b,sizeof(b),2)==Admission::Queued);
  for (uint32_t seq=21;seq<=24;++seq)
    assert(q.take(f,false) && f.sequence==seq);
  assert(!q.take(f,false));
  make(b,1,1,60); // new timestamp after a rebooted origin
  assert(q.offer(b,sizeof(b),2)==Admission::Queued);
  assert(q.take(f,false));
  std::puts("CompactPositionRelay host checks PASS");
  return 0;
}
