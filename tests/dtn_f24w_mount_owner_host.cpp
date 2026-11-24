#include "../firmware/shared/storage/dtn_f24w_mount_owner.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <functional>
using namespace DTNF24W;
struct Witness {Identity id{11,1,31,true,true,true};Identity view()const{return id;}};
struct Trace {std::string calls; char fail=0,lose=0; Witness* w=nullptr; std::function<void()> callback; bool mounted=false; bool slots[2]{};};
struct Backend {
 Trace& t; explicit Backend(Trace& x):t(x){}
 bool act(char a){t.calls+=a;if(t.callback)t.callback();if(t.lose==a)t.w->id.authorized=false;return t.fail!=a;}
 bool mount(){bool ok=act('M');if(ok)t.mounted=true;return ok;}
 bool open(uint8_t s,const char*,Mode){bool ok=act(char('0'+s));if(ok)t.slots[s]=true;return ok;}
 bool child(uint8_t,uint8_t s){bool ok=act('C');if(ok)t.slots[s]=true;return ok;}
 bool io(uint8_t,Action a,uint8_t*,size_t){return act(a==Action::Write?'W':'R');}
 bool flush(uint8_t){return act('F');}
 bool close(uint8_t s){bool ok=act(char('a'+s));if(ok)t.slots[s]=false;return ok;}
 bool unmount(){bool ok=act('U');if(ok)t.mounted=false;return ok;}
};
using Owner=MountOwner<Backend,Witness>;using Token=Owner::Token;
static_assert(!std::is_copy_constructible<Owner>::value && !std::is_move_constructible<Owner>::value,"single owner");
static_assert(!std::is_copy_constructible<Token>::value && !std::is_move_assignable<Token>::value,"no raw aliases");
static_assert(std::is_move_constructible<Token>::value && std::is_trivially_destructible<Token>::value,"no destructor I/O");
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Fixture {Witness w;Trace t;Owner o;Fixture():o(w,t){t.w=&w;}};
struct Empty {explicit operator bool()const{return false;}};
int main(){
 uint8_t buf[2]{};
 {DenyMountFacade<Empty> d; CHECK(d.state()==State::Detached);CHECK(!d.begin(47));CHECK(!d.open("/x","a"));CHECK(!d.exists("/x"));CHECK(!d.mkdir("/x"));CHECK(!d.remove("/x"));CHECK(!d.rename("/x","/y"));CHECK(d.cardSize()==0);CHECK(!d.end());CHECK(d.state()==State::Detached);}
 {Fixture f; f.w.id.authorized=false;CHECK(!f.o.mount());CHECK(f.t.calls.empty());CHECK(f.o.state()==State::Detached);f.w.id.authorized=true;CHECK(f.o.mount());CHECK(f.t.calls=="M");CHECK(!f.o.mount());CHECK(!f.o.open(Lane::None,"/x",Mode::Read));CHECK(!f.o.open(static_cast<Lane>(255),"/x",Mode::Read));CHECK(!f.o.open(Lane::History,"/x",Mode::Append));CHECK(!f.o.open(Lane::LegacyAppend,"/x",Mode::Read));CHECK(!f.o.open(Lane::Sos,nullptr,Mode::Read));CHECK(!f.o.open(Lane::Sos,"/x",static_cast<Mode>(255)));CHECK(f.t.calls=="M");
 auto a=f.o.open(Lane::Sos,"/sos",Mode::Append);CHECK(a);CHECK(f.o.outstanding()==1);CHECK(f.o.lane()==Lane::Sos);CHECK(!f.o.open(Lane::Export,"/x",Mode::Read));CHECK(!f.o.child(a));CHECK(!f.o.io(a,Action::Read,buf,2));CHECK(!f.o.io(a,Action::Write,nullptr,2));CHECK(!f.o.io(a,Action::Write,buf,0));CHECK(f.o.io(a,Action::Write,buf,2));auto moved=std::move(a);CHECK(!a);CHECK(!f.o.close(a));CHECK(moved);CHECK(f.o.close(moved));CHECK(f.o.outstanding()==0);CHECK(f.o.lane()==Lane::None);CHECK(!f.o.close(moved));CHECK(f.t.calls=="M0WFa");
 auto b=f.o.open(Lane::History,"/dir",Mode::Read);CHECK(b);CHECK(!f.o.io(moved,Action::Write,buf,2));auto child=f.o.child(b);CHECK(child);CHECK(f.o.outstanding()==2);CHECK(!f.o.close(b));CHECK(!f.o.child(child));CHECK(!f.o.child(b));CHECK(f.o.io(child,Action::Read,buf,2));CHECK(f.o.shutdown());CHECK(f.o.state()==State::Detached);CHECK(f.o.outstanding()==0);CHECK(f.t.calls=="M0WFa0CRbaU");CHECK(!f.t.mounted);CHECK(!f.o.mount());f.w.id.epoch++;CHECK(f.o.mount());auto next=f.o.open(Lane::Audit,"/x",Mode::Read);CHECK(next);CHECK(!f.o.close(b));CHECK(f.o.close(next));CHECK(f.o.shutdown());}
 {Fixture f;CHECK(f.o.mount());{auto a=f.o.open(Lane::F23D,"/journal",Mode::Append);CHECK(a);CHECK(!f.o.child(a));auto b=f.o.child(a,"/journal");CHECK(b);CHECK(!f.o.close(a));}CHECK(f.o.outstanding()==2);CHECK(f.t.calls=="M01");CHECK(f.o.shutdown());CHECK(f.t.calls=="M01bFaU");CHECK(f.o.outstanding()==0);}
 {Fixture f,g;CHECK(f.o.mount());CHECK(g.o.mount());auto a=f.o.open(Lane::Audit,"/x",Mode::Read);auto b=g.o.open(Lane::Audit,"/x",Mode::Read);CHECK(a);CHECK(b);CHECK(!g.o.close(a));CHECK(!f.o.io(b,Action::Read,buf,1));CHECK(f.o.shutdown());CHECK(g.o.shutdown());}
 // Every uncertain backend result latches fault and blocks further I/O/unmount.
 for(char fail:std::string("M0WF aU")){if(fail==' ')continue;Fixture f;f.t.fail=fail;if(fail=='M'){CHECK(!f.o.mount());}else{CHECK(f.o.mount());auto a=f.o.open(Lane::Sos,"/x",Mode::Append);if(fail=='0'){CHECK(!a);CHECK(f.o.outstanding()==1);}else{CHECK(a);if(fail=='W'){CHECK(!f.o.io(a,Action::Write,buf,1));}else CHECK(!f.o.shutdown());CHECK(f.o.outstanding()==(fail=='U'?0u:1u));}}
 CHECK(f.o.state()==State::Faulted);auto calls=f.t.calls;CHECK(!f.o.mount());CHECK(!f.o.shutdown());CHECK(!f.o.open(Lane::Sos,"/x",Mode::Append));CHECK(f.t.calls==calls);}
 for(char lose:std::string("M0WFaU")){Fixture f;f.t.lose=lose;if(lose=='M'){CHECK(!f.o.mount());}else{CHECK(f.o.mount());auto a=f.o.open(Lane::Sos,"/x",Mode::Append);if(lose=='0'){CHECK(!a);CHECK(f.o.outstanding()==1);}else{CHECK(a);if(lose=='W'){CHECK(!f.o.io(a,Action::Write,buf,1));}else CHECK(!f.o.shutdown());CHECK(f.o.outstanding()==(lose=='U'?0u:1u));}}
 CHECK(f.o.state()==State::Faulted);auto calls=f.t.calls;f.w.id.authorized=true;CHECK(!f.o.shutdown());CHECK(!f.o.mount());CHECK(f.t.calls==calls);}
 for(char action:std::string("Cb")){Fixture f;CHECK(f.o.mount());auto a=f.o.open(Lane::History,"/dir",Mode::Read);CHECK(a);f.t.fail=action;auto b=f.o.child(a);if(action=='C'){CHECK(!b);}else{CHECK(b);CHECK(!f.o.shutdown());}CHECK(f.o.state()==State::Faulted);CHECK(f.o.outstanding()==2);CHECK(f.t.calls.find('U')==std::string::npos);}
 for(unsigned kind=0;kind<6;++kind){Fixture f;CHECK(f.o.mount());auto a=f.o.open(Lane::Sos,"/x",Mode::Append);CHECK(a);switch(kind){case 0:f.w.id.media++;break;case 1:f.w.id.epoch++;break;case 2:f.w.id.owner++;break;case 3:f.w.id.authorized=false;break;case 4:f.w.id.exclusive=false;break;default:f.w.id.allPathsInterposed=false;}CHECK(!f.o.close(a));CHECK(f.o.state()==State::Faulted);CHECK(f.o.outstanding()==1);CHECK(f.t.calls=="M0");}
 {Fixture f;CHECK(f.o.mount());auto a=f.o.open(Lane::Sos,"/x",Mode::Append);CHECK(a);unsigned reentries=0;f.t.callback=[&]{++reentries;CHECK(!f.o.mount());CHECK(!f.o.close(a));CHECK(!f.o.shutdown());CHECK(!f.o.open(Lane::Audit,"/x",Mode::Read));};CHECK(f.o.shutdown());CHECK(reentries==3);CHECK(f.t.calls=="M0FaU");CHECK(f.o.state()==State::Detached);}
 {Fixture f;f.w.id.exclusive=false;CHECK(!f.o.mount());f.w.id.exclusive=true;f.w.id.allPathsInterposed=false;CHECK(!f.o.mount());CHECK(f.t.calls.empty());}
 {Fixture f;CHECK(f.o.mount());auto a=f.o.open(Lane::Export,"/x",Mode::Read);CHECK(a);f.t.fail='R';CHECK(!f.o.io(a,Action::Read,buf,1));CHECK(f.o.state()==State::Faulted);CHECK(f.o.outstanding()==1);CHECK(!f.o.shutdown());CHECK(f.t.calls=="M0R");}
 {Fixture f;CHECK(f.o.mount());auto a=f.o.open(Lane::History,"/x",Mode::Read);CHECK(a);f.t.lose='C';CHECK(!f.o.child(a));CHECK(f.o.state()==State::Faulted);CHECK(f.o.outstanding()==2);CHECK(!f.o.shutdown());CHECK(f.t.calls=="M0C");}
 std::printf("F24W owner-held mount lifecycle PASS: %u checks; synthetic backend only; no physical owner\n",checks);
}
