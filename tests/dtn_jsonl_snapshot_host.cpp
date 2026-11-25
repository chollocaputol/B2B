#include "../firmware/shared/storage/dtn_jsonl_snapshot.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace DTNJsonlSnapshot;
struct Source {
  std::string bytes;
  uint64_t failureAt=UINT64_MAX;
  uint64_t size()const{return bytes.size();}
  bool readByte(uint64_t off,uint8_t& b)const{
    if(off==failureAt || off>=bytes.size())return false;
    b=static_cast<uint8_t>(bytes[static_cast<size_t>(off)]);
    return true;
  }
};
int main(){
 assert(validSession(2,"S400ADB0"));
 assert(validSession(3,"SB36DBBB"));
 assert(!validSession(0,"S400ADB0"));
 assert(!validSession(2,"s400adb0"));
 assert(!validSession(2,"S400ADB0/"));
 assert(!validSession(2,"S400AdB0"));
 assert(std::string(filename(Journal::SosEvents))=="sos_events.jsonl");
 assert(std::string(filename(Journal::Telemetry))=="telemetry.jsonl");
 Source s{"{\"kind\":\"sos\",\"vessel_id\":2}\n{\"kind\":\"weather\"}\r\n{\"kind\":\"partial\""};
 Cursor c{};Record r{};char line[kMaxLineBytes+1U]={};
 const uint64_t frozen=s.size();
 assert(next(s,frozen,c,line,sizeof(line),r)==Outcome::Record);
 assert(r.ordinal==0U&&r.start==0U&&r.length==28U);
 assert(std::string(line)=="{\"kind\":\"sos\",\"vessel_id\":2}");
 const uint32_t h=r.fnv1a;
 assert(h==fnv(line,r.length));
 assert(next(s,frozen,c,line,sizeof(line),r)==Outcome::Record);
 assert(r.ordinal==1U && r.start==29U && std::string(line)=="{\"kind\":\"weather\"}");
 const Cursor prior=c;
 assert(next(s,frozen,c,line,sizeof(line),r)==Outcome::IncompleteTail);
 assert(c.offset==prior.offset&&c.ordinal==prior.ordinal);
 s.bytes+="}\n";
 assert(next(s,frozen,c,line,sizeof(line),r)==Outcome::IncompleteTail); // old snapshot cannot grow
 assert(next(s,s.size(),c,line,sizeof(line),r)==Outcome::Record);
 assert(r.ordinal==2U && std::string(line)=="{\"kind\":\"partial\"}");
 assert(next(s,s.size(),c,line,sizeof(line),r)==Outcome::End);
 Source invalid{"{broken}\n"};Cursor bad{};
 assert(next(invalid,invalid.size(),bad,line,sizeof(line),r)==Outcome::Record);
 // Braces alone are NOT a JSON parse: backend mapping still required.
 Source malformed{"[not-a-json-object]\n"};
 bad=Cursor{};
 assert(next(malformed,malformed.size(),bad,line,sizeof(line),r)==Outcome::Malformed);
 assert(bad.offset==0U);
 Source newline{"{}\n{\"x\":\"bad\tcontrol\"}\n"};Cursor control{};
 assert(next(newline,newline.size(),control,line,sizeof(line),r)==Outcome::Record);
 assert(next(newline,newline.size(),control,line,sizeof(line),r)==Outcome::Malformed);
 assert(control.ordinal==1U);
 Source longline{std::string(kMaxLineBytes+5U,'A')+"\n"};Cursor big{};
 assert(next(longline,longline.size(),big,line,sizeof(line),r)==Outcome::Oversize);
 assert(big.offset==0U);
 Source io{"{}\n"};io.failureAt=1U;Cursor readError{};
 assert(next(io,io.size(),readError,line,sizeof(line),r)==Outcome::IoError);
 assert(readError.offset==0U);
 assert(next(io,io.size()+1U,readError,line,sizeof(line),r)==Outcome::IoError);
 assert(next(io,io.size(),readError,nullptr,0,r)==Outcome::InvalidInput);
 Source empty{};Cursor zero{};
 assert(next(empty,0,zero,line,sizeof(line),r)==Outcome::End);
 std::puts("DTN F06 JSONL immutable snapshot framing: PASS");
}
