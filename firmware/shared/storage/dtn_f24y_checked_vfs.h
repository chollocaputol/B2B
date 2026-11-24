#pragma once
// F24Y: descriptor backend for F24W/F24X. No raw descriptors escape; no destructor I/O.
// Driver supplies mount/unmount status and a fixed VFS root. This is NOT authority.
#include "dtn_f24w_mount_owner.h"
#include "dtn_f24o_owned_sos_port.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <limits>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
namespace DTNF24Y {
using DTNF24W::Mode; using DTNF24W::Action;
enum class FileKind:uint8_t {Missing, Regular, Directory, Error};
// Only the existing two journal names, exact vessel/session grammar.
inline bool journalPath(const char* path){
 bool current=false;
 for(uint32_t vessel=1;vessel<=3;++vessel)
  if(DTNF24O::supportedPath(path,"",vessel,current))return true;
 return false;
}
struct NativeVfs {
#ifdef _WIN32
 using Stat=struct _stat64;
 static int pathStat(const char* p,Stat* s){return ::_stat64(p,s);}
 static int fdStat(int fd,Stat* s){return ::_fstat64(fd,s);}
 static bool regular(const Stat& s){return (s.st_mode&_S_IFMT)==_S_IFREG;}
 static bool directory(const Stat& s){return (s.st_mode&_S_IFMT)==_S_IFDIR;}
 static int open(const char* p,Mode m){return ::_open(p,(m==Mode::Append?O_WRONLY|O_CREAT|O_APPEND:O_RDONLY)|O_BINARY,_S_IREAD|_S_IWRITE);}
 static int64_t read(int fd,uint8_t* p,size_t n){return ::_read(fd,p,unsigned(n));}
 static int64_t write(int fd,const uint8_t* p,size_t n){return ::_write(fd,p,unsigned(n));}
 static int64_t seek(int fd,size_t n){return ::_lseeki64(fd,static_cast<int64_t>(n),SEEK_SET);}
 static int sync(int fd){return ::_commit(fd);}
 static int close(int fd){return ::_close(fd);}
#else
 using Stat=struct stat;
 static int pathStat(const char* p,Stat* s){return ::stat(p,s);}
 static int fdStat(int fd,Stat* s){return ::fstat(fd,s);}
 static bool regular(const Stat& s){return S_ISREG(s.st_mode);}
 static bool directory(const Stat& s){return S_ISDIR(s.st_mode);}
 static int open(const char* p,Mode m){return ::open(p,m==Mode::Append?O_WRONLY|O_CREAT|O_APPEND:O_RDONLY,0600);}
 static int64_t read(int fd,uint8_t* p,size_t n){return ::read(fd,p,n);}
 static int64_t write(int fd,const uint8_t* p,size_t n){return ::write(fd,p,n);}
 static int64_t seek(int fd,size_t n){if(uint64_t(n)>uint64_t(INT64_MAX)||uint64_t(n)>uint64_t((std::numeric_limits<off_t>::max)()))return -1;return ::lseek(fd,static_cast<off_t>(n),SEEK_SET);}
 static int sync(int fd){return ::fsync(fd);}
 static int close(int fd){return ::close(fd);}
#endif
 // Exclusive directory creation is host-only. ESP32 never creates a session here.
 static bool createDirectory(const char* p){
#if defined(ARDUINO) || defined(ESP_PLATFORM)
  (void)p;return false;
#elif defined(_WIN32)
  return ::_mkdir(p)==0;
#else
  return ::mkdir(p,0700)==0;
#endif
 }
 static DIR* dirOpen(const char* p){return ::opendir(p);}
 static struct dirent* dirNext(DIR* d){return ::readdir(d);}
 static int dirClose(DIR* d){return ::closedir(d);}
 static FileKind kind(const char* p){Stat s{};errno=0;if(pathStat(p,&s)!=0)return errno==ENOENT?FileKind::Missing:FileKind::Error;
 return regular(s)?FileKind::Regular:directory(s)?FileKind::Directory:FileKind::Error;}
 static bool size(int fd,size_t& n){Stat s{};n=0;if(fdStat(fd,&s)!=0 || !regular(s) || s.st_size<0 || uint64_t(s.st_size)>uint64_t(SIZE_MAX))return false;n=size_t(s.st_size);return true;}
};
// Single serialized caller required. Failures are sticky, including uncertain close.
// Missing parent, nonregular paths, short reads/writes and ambiguous metadata fail.
// Existing F24W owner handles tokens; F24Z adds a bounded discovery snapshot.
template<class Driver,class Sys=NativeVfs> class Backend final {
 Driver driver_; int fd_[2]{-1,-1}; Mode mode_[2]{Mode::Read,Mode::Read};
 DIR* directory_=nullptr; // F24Z: retained on uncertain teardown, never destructor I/O.
 bool mounted_=false, fault_=false;
 bool fail(){fault_=true;return false;}
 bool active()const{return mounted_&&!fault_;}
 bool slot(uint8_t s)const{return active()&&s<2&&fd_[s]>=0;}
 bool full(const char* path,char (&out)[512])const{
  if(!journalPath(path))return false;
  const char* r=driver_.root();if(!r || !*r)return false;
  const int n=snprintf(out,sizeof(out),"%s%s",r,path);return n>0&&size_t(n)<sizeof(out);
 }
 bool parent(char (&path)[512]){
  char* p=strrchr(path,'/');if(!p)return false;
  *p=0;const auto kind=Sys::kind(path);*p='/';return kind==FileKind::Directory;
 }
 public:
 template<class... Args> explicit Backend(Args&&... args):driver_(std::forward<Args>(args)...){ }
 Backend(const Backend&)=delete;Backend& operator=(const Backend&)=delete;
 bool mount(){if(mounted_||fault_)return false;if(!driver_.mount())return fail();mounted_=true;return true;}
 // F25B: only after owner mount; no recursive mkdir or existing-directory reuse.
 bool createSession(uint32_t vessel,const char* current){
#if defined(ARDUINO) || defined(ESP_PLATFORM)
  (void)vessel;(void)current;return fail();
#else
  char journal[96]{},path[512]{};
  if(!active() || fd_[0]>=0 || fd_[1]>=0 || directory_ || !DTNF24M::sandboxPath(journal,sizeof(journal),current,vessel) || !full(journal,path))return fail();
  char* slash=strrchr(path,'/');if(!slash)return fail();*slash=0;
  if(!parent(path) || Sys::kind(path)!=FileKind::Missing || !Sys::createDirectory(path) || Sys::kind(path)!=FileKind::Directory)return fail();
  return true;
#endif
 }
 bool exists(const char* path,bool& found){
  found=false;char p[512]{};if(!active()||fd_[0]>=0||fd_[1]>=0)return false;
  if(!full(path,p)||!parent(p))return fail();
  const auto kind=Sys::kind(p);if(kind==FileKind::Missing)return true;
  if(kind!=FileKind::Regular)return fail();
  found=true;return true;
 }
 bool open(uint8_t s,const char* path,Mode m){
  char p[512]{};if(!active()||s>=2||fd_[s]>=0)return false;
  if((m!=Mode::Append&&m!=Mode::Read)||!full(path,p)||!parent(p))return fail();
  const auto kind=Sys::kind(p);if(kind!=FileKind::Regular && !(kind==FileKind::Missing&&m==Mode::Append))return fail();
  fd_[s]=Sys::open(p,m);mode_[s]=m;if(fd_[s]<0)return fail();
  size_t ignored=0;return Sys::size(fd_[s],ignored)||fail();
 }
 // F24Z checked discovery. Current session must exist. Only exact old
 // session directories enter the snapshot; a trusted closure witness is required.
 template<class Guard> bool discover(uint32_t vessel,const char* current,DTNF24W::SessionSnapshot& out,const Guard& allowed){
  out={};char journal[96]{},logical[48]{},path[512]{};
  if(!active()||fd_[0]>=0||fd_[1]>=0||directory_||!allowed())return fail();
  if(!DTNF24M::sandboxPath(journal,sizeof(journal),current,vessel))return fail();
  const int n=snprintf(logical,sizeof(logical),"/THESIS/F06TEST/V%03lu",(unsigned long)vessel);
  const char* root=driver_.root();if(!root||!*root||n<=0||size_t(n)>=sizeof(logical))return fail();
  const int k=snprintf(path,sizeof(path),"%s%s",root,logical);
  if(k<=0||size_t(k)>=sizeof(path)||Sys::kind(path)!=FileKind::Directory||!allowed())return fail();
  directory_=Sys::dirOpen(path);if(!directory_||!allowed())return fail();
  DTNF24W::SessionSnapshot copy{};bool exhausted=false,foundCurrent=false,good=true;
  for(unsigned i=0;i<64;++i){
   if(!allowed())return fail();
   errno=0;const auto* entry=Sys::dirNext(directory_);const int err=errno;
   if(!allowed())return fail();
   if(!entry){exhausted=err==0;break;}
   const char* name=entry->d_name;
   if(!strcmp(name,".")||!strcmp(name,".."))continue;
   if(!*name||strchr(name,'/')||strchr(name,'\\')){good=false;break;}
   char childPath[512]{};const int m=snprintf(childPath,sizeof(childPath),"%s/%s",path,name);
   if(m<=0||size_t(m)>=sizeof(childPath)){good=false;break;}
   const auto kind=Sys::kind(childPath);if(!allowed())return fail();
   const bool session=DTNF24M::validSession(name);
   if(kind==FileKind::Regular&&!session)continue;
   if(kind!=FileKind::Directory||!session){good=false;break;}
   char candidate[64]{};const int q=snprintf(candidate,sizeof(candidate),"%s/%s",logical,name);
   if(q<=0||size_t(q)>=sizeof(candidate)){good=false;break;}
   if(!strcmp(candidate,current)){if(foundCurrent){good=false;break;}foundCurrent=true;continue;}
   if(copy.count>=12){good=false;break;}
   for(size_t j=0;j<copy.count;++j)if(!strcmp(copy.paths[j],candidate))good=false;
   if(!good)break;
   memcpy(copy.paths[copy.count++],candidate,size_t(q)+1);
  }
  if(!allowed()||Sys::dirClose(directory_)!=0)return fail();
  directory_=nullptr;
  if(!allowed()||!good||!exhausted||!foundCurrent)return fail();
  // Stable lexical order for deterministic recovery, not chronology proof.
  for(size_t i=1;i<copy.count;++i)for(size_t j=i;j>0&&strcmp(copy.paths[j-1],copy.paths[j])>0;--j){
   char tmp[64];memcpy(tmp,copy.paths[j-1],64);memcpy(copy.paths[j-1],copy.paths[j],64);memcpy(copy.paths[j],tmp,64);
  }
  out=copy;return true;
 }
 bool child(uint8_t,uint8_t){return fail();} // Directory traversal not migrated.
 bool io(uint8_t s,Action a,uint8_t* p,size_t n){
  if(!slot(s))return false;
  if(!p||!n||n>size_t(INT_MAX))return fail();
  int64_t got=-1;
  if(a==Action::Read&&mode_[s]==Mode::Read)got=Sys::read(fd_[s],p,n);
  else if(a==Action::Write&&mode_[s]==Mode::Append)got=Sys::write(fd_[s],p,n);
  else return fail();
  // Never retry an ambiguous or partial operation.
  return got==int64_t(n)||fail();
 }
 bool size(uint8_t s,size_t& n){n=0;if(!slot(s))return false;size_t v=0;if(!Sys::size(fd_[s],v))return fail();n=v;return true;}
 bool seek(uint8_t s,size_t n){if(!slot(s))return false;if(mode_[s]!=Mode::Read||uint64_t(n)>uint64_t(INT64_MAX))return fail();return Sys::seek(fd_[s],n)==int64_t(n)||fail();}
 bool flush(uint8_t s){if(!slot(s))return false;if(mode_[s]!=Mode::Append)return fail();return Sys::sync(fd_[s])==0||fail();}
 bool close(uint8_t s){if(!slot(s))return false;if(Sys::close(fd_[s])!=0)return fail();fd_[s]=-1;return true;}
 bool unmount(){if(!active()||fd_[0]>=0||fd_[1]>=0||directory_)return false;if(!driver_.unmount())return fail();mounted_=false;return true;}
};
} // namespace DTNF24Y
