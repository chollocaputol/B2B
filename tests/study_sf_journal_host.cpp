// Linux host test of the same encoder, file journal and confirmation parser as target.
#include "../firmware/shared/storage/study_sf_journal.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
using namespace StudySF;
static constexpr const char* a="aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",*b="bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",*c="cccccccc-cccc-4ccc-8ccc-cccccccccccc",*batch="dddddddd-dddd-4ddd-8ddd-dddddddddddd";
static unsigned checks=0;
static std::string batchFor(unsigned n){char out[37];snprintf(out,sizeof(out),"dddddddd-dddd-4ddd-8ddd-%012u",n+1);return out;}
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static std::string read(const std::string& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void write(const std::string& p,const std::string& text){std::ofstream f(p,std::ios::binary);f<<text;assert(f.good());}
static std::string receipt(const Batch& x,const char* status="accepted"){std::string s="{\"records\":[";for(size_t i=0;i<x.count;++i){if(i)s+=',';s+="{\"status\":\""+std::string(status)+"\",\"record_id\":\""+x.ids[i]+"\"}";}return s+"],\"batch_id\":\""+x.id+"\",\"recorder_node_id\":3,\"schema_version\":1}";}
static bool generate(Encoder& e,Record& r,unsigned i){if(i%4==0)return e.weather(r,i*1000,"2026-11-25T00:00:00Z",29,78,1009,-1,1,"NO_PATTERN","NONE",true);if(i%4==1)return e.gps(r,i*1000,nullptr,i%8==1,8.31,124.2);VesselPacket p{};p.magic=PACKET_MAGIC;p.version=PACKET_VERSION;p.packetType=i%4==2?PACKET_SOS:PACKET_SOS_CANCEL;p.originNodeId=2;p.sequenceNumber=i/4+1;if(p.packetType==PACKET_SOS_CANCEL)encodeSOSAck(p,p.sequenceNumber,p.originNodeId);return e.control(r,i*1000,nullptr,p,1);}
int main(int argc,char** argv){if(argc!=3)return 2;std::string mode=argv[1],root=argv[2],old=root+"/study_sf/v0003/"+a+".jsonl",next=root+"/study_sf/v0003/"+b+".jsonl";
 if(mode=="seed"){
  CHECK(std::filesystem::is_directory(root));Encoder e;Journal j;CHECK(e.begin(3,a));CHECK(j.begin(root.c_str(),3,a));Record r;CHECK(!e.control(r,1,nullptr,VesselPacket{},1));CHECK(!e.weather(r,1,nullptr,1,2,3,4,7));CHECK(!utcValid("2026-02-30T00:00:00Z"));CHECK(utcValid("2024-02-29T00:00:00Z"));
  for(unsigned i=0;i<40;++i){CHECK(generate(e,r,i));CHECK(j.append(r));}
  const auto before=read(old);CHECK(j.prepare(batch)==Journal::Next::Ready);CHECK(j.batch().count==8);const auto first=std::string(j.batch().body);CHECK(!j.confirm(503,receipt(j.batch()).c_str()));CHECK(!j.confirm(200,"{}"));auto wrongNode=receipt(j.batch());wrongNode.replace(wrongNode.find("recorder_node_id\":3")+18,1,"4");CHECK(!j.confirm(200,wrongNode.c_str()));auto wrongBatch=receipt(j.batch());wrongBatch.replace(wrongBatch.find(batch),36,c);CHECK(!j.confirm(200,wrongBatch.c_str()));CHECK(j.prepare(c)==Journal::Next::Ready);CHECK(first==j.batch().body);CHECK(!j.confirm(200,receipt(j.batch(),"pending").c_str()));auto wrong=receipt(j.batch());wrong.replace(wrong.find(a),36,c);CHECK(!j.confirm(200,wrong.c_str()));CHECK(j.confirm(200,receipt(j.batch(),"duplicate").c_str()));CHECK(j.prepare(batch)==Journal::Next::Ready);CHECK(first!=j.batch().body);CHECK(read(old)==before);Journal collision;CHECK(!collision.begin(root.c_str(),3,a));CHECK(read(old)==before);
  // A deliberately incomplete final line, as from interrupted append. Do not modify valid prefix.
  std::ofstream f(old,std::ios::app|std::ios::binary);f<<"{\"record_id\":";f.close();CHECK(read(old)==before+"{\"record_id\":");
 } else if(mode=="restore"){
  Encoder e;Journal j;CHECK(e.begin(3,b));CHECK(j.begin(root.c_str(),3,b));Record r;for(unsigned i=0;i<8;++i){CHECK(generate(e,r,i));CHECK(j.append(r));}const auto prev=read(old),cur=read(next);unsigned seen=0,files=0,loops=0;while(++loops<30){auto n=j.prepare(batchFor(files).c_str());if(n==Journal::Next::Idle)break;if(n==Journal::Next::Advanced||n==Journal::Next::Torn)continue;CHECK(n==Journal::Next::Ready);CHECK(j.batch().count<=8);write(root+"/request"+std::to_string(files++)+".json",j.batch().body);seen+=j.batch().count;CHECK(j.confirm(200,receipt(j.batch()).c_str()));}CHECK(seen==48);CHECK(files==6);CHECK(j.tornFiles()==1);CHECK(read(old)==prev);CHECK(read(next)==cur);CHECK(j.prepare(batch)==Journal::Next::Idle);CHECK(generate(e,r,8));CHECK(j.append(r));CHECK(j.prepare(batch)==Journal::Next::Ready);CHECK(j.batch().count==1);CHECK(j.confirm(200,receipt(j.batch()).c_str()));
  // New final record is not part of the six fixture batches.
 } else if(mode=="actual"){
  Journal j;CHECK(j.begin(root.c_str(),3,c));unsigned files=0,seen=0,loops=0;while(++loops<30){auto n=j.prepare(batchFor(files).c_str());if(n==Journal::Next::Idle)break;if(n==Journal::Next::Advanced||n==Journal::Next::Torn)continue;CHECK(n==Journal::Next::Ready);if(files<6){CHECK(read(root+"/request"+std::to_string(files)+".json")==j.batch().body);auto response=read(root+"/response"+std::to_string(files)+".json");CHECK(j.confirm(200,response.c_str()));}else CHECK(j.confirm(200,receipt(j.batch()).c_str()));seen+=files<6?8:1;++files;}CHECK(seen==49);CHECK(files==7);CHECK(j.tornFiles()==1);
 }else return 2;
 printf("STUDY SF %s PASS: %u checks\n",mode.c_str(),checks);return 0;
}
