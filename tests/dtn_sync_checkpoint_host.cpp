#include "../firmware/shared/storage/dtn_sync_checkpoint.h"
#include <cassert>
#include <cstdio>
using namespace DTNSyncCheckpoint;
struct MockStore {
 bool present[2]={false,false};
 Snapshot slots[2]{};
 bool failWrite=false,corruptWrite=false,readError=false;
 SlotRead read(unsigned i,Snapshot& out){
   if(readError)return SlotRead::Error;
   if(!present[i])return SlotRead::Missing;
   out=slots[i];return SlotRead::Present;
 }
 bool write(unsigned i,const Snapshot& next){
   if(failWrite)return false;
   slots[i]=next;present[i]=true;
   if(corruptWrite)slots[i].check^=1U;
   return true;
 }
};
int main(){
 const uint64_t session=0xAABBCCDD55667788ULL;
 MockStore st;
 Cursor<MockStore> cur;
 assert(cur.load(st,session)==Load::Fresh);
 assert(!cur.ready());
 assert(cur.initializeFresh(st,session));
 assert(cur.ready()&&cur.nextRecord()==0U);
 assert(cur.load(st,session)==Load::Loaded); // reload durable state
 const Reply partial[]={Reply::Accepted,Reply::Duplicate,Reply::Retry,Reply::Accepted};
 assert(!cur.commitReply(st,session,0U,partial,4,false,true));
 assert(!cur.commitReply(st,session,0U,partial,4,true,false));
 assert(!cur.commitReply(st,session,1U,partial,4,true,true));
 assert(cur.nextRecord()==0U);
 assert(cur.commitReply(st,session,0U,partial,4,true,true));
 assert(cur.nextRecord()==2U); // later accepted item MUST NOT skip gap
 const Reply retry[]={Reply::Duplicate,Reply::Accepted};
 assert(cur.commitReply(st,session,2U,retry,2,true,true));
 assert(cur.nextRecord()==4U);
 assert(!cur.commitReply(st,session,0U,retry,2,true,true));
 Cursor<MockStore> reboot;
 assert(reboot.load(st,session)==Load::Loaded && reboot.nextRecord()==4U);
 assert(reboot.load(st,session+1U)==Load::WrongSession);
 assert(reboot.load(st,session)==Load::Loaded);
 st.failWrite=true;
 assert(!reboot.commitReply(st,session,4U,retry,2,true,true));
 assert(!reboot.ready());
 st.failWrite=false;
 Cursor<MockStore> safe;
 assert(safe.load(st,session)==Load::Loaded && safe.nextRecord()==4U);
 st.corruptWrite=true;
 assert(!safe.commitReply(st,session,4U,retry,2,true,true));
 st.corruptWrite=false;
 Cursor<MockStore> recovered;
 assert(recovered.load(st,session)==Load::Loaded && recovered.nextRecord()==4U);
 st.slots[0].check^=3U;st.slots[1].check^=5U;
 assert(recovered.load(st,session)==Load::Corrupt);
 MockStore io;io.readError=true;
 assert(recovered.load(io,session)==Load::IoError);
 MockStore wrong;
 wrong.present[0]=true;wrong.slots[0]=make(session+1U,1U,0U);
 assert(recovered.load(wrong,session)==Load::WrongSession);
 MockStore rejected;
 assert(recovered.load(rejected,session)==Load::Fresh);
 assert(recovered.initializeFresh(rejected,session));
 const Reply bad[]={Reply::Rejected,Reply::Accepted};
 assert(!recovered.commitReply(rejected,session,0U,bad,2,true,true));
 assert(recovered.nextRecord()==0U);
 static_assert(kMaxBatch==32U,"bounded batches");
 std::puts("DTN offline checkpoint/partial ACK/power loss/collision: PASS");
}
