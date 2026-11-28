#pragma once
// F24T: bounded read-only F06 history listing through F24R's two-slot
// directory File adapter. This is a SOFTWARE MODEL. The only F24T firmware
// witness is F24Q::DenyWitness, so no raw SD File can open on a board.
// Real Arduino File teardown after revocation is NOT approved by this code.
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace DTNF24T {
static constexpr unsigned kMaxVisited=64;
static constexpr unsigned kMaxShown=48;
struct Result {
 bool opened=false, complete=false, truncated=false, faulted=false;
 unsigned visited=0, shown=0;
};
inline bool validRoot(const char* root,uint32_t vessel){
 if(!root || vessel<1U || vessel>3U)return false;
 static const char prefix[]="/THESIS/F06TEST/V00";
 const size_t length=sizeof(prefix)-1U;
 return strncmp(root,prefix,length)==0 &&
        root[length]==char('0'+vessel) && root[length+1]=='\0';
}
inline bool validSession(const char* str){
 if(!str || str[0]!='S')return false;
 for(unsigned i=1;i<8;++i){
  const char c=str[i];
  if(c=='\0' || !((c>='0' && c<='9') || (c>='A' && c<='F')))return false;
 }
 return str[8]=='\0';
}
// Reject foreign prefixes and any path with a second slash after the root.
// Name is copied to an internal eight-character buffer before emitting.
inline bool childName(const char* raw,const char* root,char (&out)[9]){
 if(!raw || !root)return false;
 size_t n=0;
 while(n<96U && raw[n]!='\0')++n;
 if(n==0 || n==96U)return false;
 const char* part=raw;
 if(raw[0]=='/'){
  const size_t rn=strlen(root);
  if(n<=rn+1U || strncmp(raw,root,rn)!=0 || raw[rn]!='/')return false;
  part=raw+rn+1U;
 }
 if(!validSession(part))return false;
 memcpy(out,part,9U);
 return true;
}
template<class GuardedVolume> class HistoryDirectory final {
 public:
 explicit HistoryDirectory(GuardedVolume& volume):volume_(volume){}
 HistoryDirectory(const HistoryDirectory&)=delete;
 HistoryDirectory& operator=(const HistoryDirectory&)=delete;
 template<class Sink> Result scan(const char* root,uint32_t vessel,Sink&& sink){
  Result result{};
  if(!validRoot(root,vessel))return result;
  auto parent=volume_.open(root,"r");
  if(!parent){result.faulted=volume_.faulted();return result;}
  result.opened=true;
  if(!parent.isDirectory()){
    parent.close();result.faulted=volume_.faulted();return result;
  }
  // Attempt at most 65 child opens: 64 bounded entries + 1 overflow probe.
  // Opening an invalid child may mean EOF or backend failure; no durable SOS
  // closure/completeness claim is permitted from this diagnostic listing.
  for(unsigned i=0;i<=kMaxVisited;++i){
   auto child=parent.openNextFile();
   if(!child){
    result.faulted=volume_.faulted();
    if(!result.faulted)result.complete=true;
    break;
   }
   if(i==kMaxVisited){result.truncated=true;child.close();break;}
   ++result.visited;
   const bool dir=child.isDirectory();
   const char* raw=child.name();
   char session[9]{};
   const bool candidate=dir && childName(raw,root,session);
   const bool bad=volume_.faulted();
   child.close();
   if(bad || volume_.faulted()){
    result.faulted=true;break;
   }
   if(!candidate)continue;
   if(result.shown==kMaxShown){result.truncated=true;continue;}
   sink(session);
   ++result.shown;
  }
  if(!volume_.faulted())parent.close();
  result.faulted = result.faulted || volume_.faulted();
  if(result.faulted)result.complete=false;
  return result;
 }
 private: GuardedVolume& volume_;
};
} // namespace DTNF24T
