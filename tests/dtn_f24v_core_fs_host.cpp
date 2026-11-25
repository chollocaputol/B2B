// F24V: actual installed Arduino-ESP32 3.3.12 FS.cpp compiled with minimal
// Stream shim. Counting FileImpl emulates backend close; VFS destructor path
// is source-verified separately. This intentionally reproduces UNSAFE paths.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>
#include "FS.h"
#include "FSImpl.h"
static unsigned checks=0;
#define C(x) do{++checks;if(!(x)){std::fprintf(stderr,"F24V real FS FAIL %u: %s\n",unsigned(__LINE__),#x);std::exit(21);}}while(0)
struct Counts {int closes=0,destroys=0,closesAfterLoss=0;bool ownerAlive=true;};
class Impl final:public fs::FileImpl {
 public:
  explicit Impl(Counts& x):c(x){}
  ~Impl()override {++c.destroys;close();}
  size_t write(const uint8_t*,size_t n)override{return n;}
  size_t read(uint8_t*,size_t)override{return 0;}
  void flush()override{}
  bool seek(uint32_t,fs::SeekMode)override{return true;}
  size_t position()const override{return 0;}
  size_t size()const override{return 0;}
  bool setBufferSize(size_t)override{return true;}
  void close()override {if(open){open=false;++c.closes;if(!c.ownerAlive)++c.closesAfterLoss;}}
  time_t getLastWrite()override{return 0;}
  const char* path()const override{return "/mock";}
  const char* name()const override{return "mock";}
  boolean isDirectory()override{return false;}
  fs::FileImplPtr openNextFile(const char*)override{return {};}
  boolean seekDir(long)override{return false;}
  String getNextFileName()override{return String();}
  String getNextFileName(bool*)override{return String();}
  void rewindDirectory()override{}
  operator bool()override{return open;}
 private: Counts& c; bool open=true;
};
int main(){
 C(std::is_copy_constructible<fs::File>::value);
 C(std::is_copy_assignable<fs::File>::value);
 C((std::is_constructible<fs::File,fs::File&&>::value));
 {Counts c{};
  {auto imp=std::make_shared<Impl>(c);fs::File first(imp);fs::File alias=first;
   C(bool(first));C(bool(alias));C(c.closes==0);C(imp.use_count()==3);
   first.close();C(c.closes==1);C(!bool(first));C(!bool(alias));
   C(imp.use_count()==2);C(c.destroys==0);
  }
  C(c.closes==1);C(c.destroys==1);C(c.closesAfterLoss==0);
 }
 {Counts c{};
  {auto imp=std::make_shared<Impl>(c);fs::File a(imp);
   {fs::File b=a;C(imp.use_count()==3);C(c.closes==0);}
   C(c.closes==0);C(imp.use_count()==2);
   c.ownerAlive=false;
  }
  // Counterexample: final shared FileImpl destruction closes post-loss.
  C(c.closes==1);C(c.destroys==1);C(c.closesAfterLoss==1);
 }
 {Counts c{};
  {auto imp=std::make_shared<Impl>(c);fs::File a(imp);
   fs::File assigned;assigned=a;C(imp.use_count()==3);
   C(bool(a));C(bool(assigned));
   // Scope end cannot be assumed to close just because File alias dies.
  }
  C(c.closes==1);C(c.destroys==1);C(c.closesAfterLoss==0);
 }
 {Counts c{};
  {auto imp=std::make_shared<Impl>(c);fs::File first(imp);
   fs::File second(std::move(first));
   // Actual implementation may move or copy; either way backend must be
   // scoped by a global owner (type-trait "movable" is not uniqueness).
   C(bool(second));C(c.closes==0);C(imp.use_count()>=2);
  }
  C(c.closes==1);C(c.destroys==1);
 }
 C(checks==34);
 std::printf("F24V actual Arduino FS.cpp shared File behavior PASS: %u checks; post-revocation destructor HAZARD reproduced\n",checks);
 return 0;
}
