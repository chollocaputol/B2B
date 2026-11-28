#pragma once
// Host-only, bounded syntax + core-field gate for original F06 JSONL records.
// This is NOT a cloud schema mapper, authenticator, or upload permission.
// All actual F06 records are flat ASCII-key JSON objects.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dtn_jsonl_snapshot.h"

namespace DTNF06RecordGate {
enum class Verdict : uint8_t {
  CandidateUnverified, DiagnosticsOnly, WrongVessel, InvalidJson,
  InvalidFields, UnsupportedInput
};
struct Fields {
  uint32_t vesselId=0, bootToken=0, eventIndex=0, uptimeMs=0;
  char kind[40]{};
};
class Parser {
 public:
  Parser(const char* s,size_t size):s_(s),size_(size){}
  bool parse(Fields& fields,bool& hasBoot,bool& hasIndex,bool& hasUp) {
    if(!s_ || !size_ || size_>DTNJsonlSnapshot::kMaxLineBytes)return false;
    space();if(!eat('{'))return false;
    space();if(eat('}'))return false; // empty object lacks identity
    char keys[48][48]{};size_t count=0;
    bool seenKind=false,seenVessel=false;
    do {
      space();char key[48]{};
      if(!keyString(key,sizeof(key)) || count==48U)return false;
      for(size_t i=0;i<count;++i)if(strcmp(keys[i],key)==0)return false;
      memcpy(keys[count++],key,strlen(key)+1U);
      space();if(!eat(':'))return false;
      space();
      if(strcmp(key,"kind")==0) {
        if(!plainString(fields.kind,sizeof(fields.kind)))return false;
        seenKind=true;
      }else if(strcmp(key,"vessel_id")==0){
        if(!unsigned32(fields.vesselId))return false;
        seenVessel=true;
      }else if(strcmp(key,"boot_token")==0){
        if(!unsigned32(fields.bootToken))return false;
        hasBoot=true;
      }else if(strcmp(key,"event_index")==0){
        if(!unsigned32(fields.eventIndex))return false;
        hasIndex=true;
      }else if(strcmp(key,"uptime_ms")==0){
        if(!unsigned32(fields.uptimeMs))return false;
        hasUp=true;
      }else if(!scalar())return false;
      space();
      if(eat('}'))break;
      if(!eat(','))return false;
    }while(true);
    space();
    if(i_!=size_ || !seenKind || !seenVessel || !fields.kind[0])return false;
    for(const char* p=fields.kind;*p;++p)
      if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='_'))return false;
    return true;
  }
 private:
  bool eat(char c){if(i_<size_ && s_[i_]==c){++i_;return true;}return false;}
  void space(){while(i_<size_ && (s_[i_]==' '||s_[i_]=='\t'||s_[i_]=='\r'||s_[i_]=='\n'))++i_;}
  static bool digit(char c){return c>='0'&&c<='9';}
  bool keyString(char* out,size_t cap){
    if(!eat('"'))return false;
    size_t n=0;
    while(i_<size_){
      const unsigned char c=static_cast<unsigned char>(s_[i_++]);
      if(c=='"'){if(!n)return false;out[n]='\0';return true;}
      if(c=='\\'||c<0x21U||c>0x7EU||n+1U>=cap)return false;
      if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
           (c>='0'&&c<='9')||c=='_'))return false;
      out[n++]=static_cast<char>(c);
    }
    return false;
  }
  bool plainString(char* out,size_t cap){
    if(!eat('"'))return false;
    size_t n=0;
    while(i_<size_){
      const unsigned char c=static_cast<unsigned char>(s_[i_++]);
      if(c=='"'){if(!n)return false;out[n]='\0';return true;}
      if(c=='\\'||c<0x20U||c>0x7EU||n+1U>=cap)return false;
      out[n++]=static_cast<char>(c);
    }
    return false;
  }
  bool generalString(){
    if(!eat('"'))return false;
    while(i_<size_){
      const unsigned char c=static_cast<unsigned char>(s_[i_++]);
      if(c=='"')return true;
      if(c<0x20U||c>0x7EU)return false; // legacy F06 ASCII; fail closed
      if(c!='\\')continue;
      if(i_>=size_)return false;
      const char e=s_[i_++];
      if(e=='"'||e=='\\'||e=='/'||e=='b'||e=='f'||e=='n'||e=='r'||e=='t')continue;
      if(e!='u'||size_-i_<4U)return false;
      for(unsigned j=0;j<4U;++j){
        const char h=s_[i_++];
        if(!((h>='0'&&h<='9')||(h>='A'&&h<='F')||(h>='a'&&h<='f')))return false;
      }
    }
    return false;
  }
  bool number(bool unsignedOnly,uint32_t* result){
    const size_t start=i_;
    if(eat('-')&&unsignedOnly)return false;
    if(i_>=size_)return false;
    if(eat('0')){
      if(i_<size_&&digit(s_[i_]))return false;
    }else{
      if(s_[i_]<'1'||s_[i_]>'9')return false;
      while(i_<size_&&digit(s_[i_]))++i_;
    }
    const size_t intEnd=i_;
    if(eat('.')){
      if(unsignedOnly||i_>=size_||!digit(s_[i_]))return false;
      while(i_<size_&&digit(s_[i_]))++i_;
    }
    if(i_<size_&&(s_[i_]=='e'||s_[i_]=='E')){
      if(unsignedOnly)return false;
      ++i_;if(i_<size_&&(s_[i_]=='+'||s_[i_]=='-'))++i_;
      if(i_>=size_||!digit(s_[i_]))return false;
      while(i_<size_&&digit(s_[i_]))++i_;
    }
    if(result){
      uint64_t v=0;
      for(size_t j=start;j<intEnd;++j){
        const unsigned n=static_cast<unsigned>(s_[j]-'0');
        if(v>(UINT32_MAX-n)/10U)return false;
        v=v*10U+n;
      }
      *result=static_cast<uint32_t>(v);
    }
    return true;
  }
  bool unsigned32(uint32_t& out){return number(true,&out);}
  bool literal(const char* text){
    const size_t n=strlen(text);
    if(n>size_-i_ || memcmp(s_+i_,text,n)!=0)return false;
    i_+=n;return true;
  }
  bool scalar(){
    if(i_>=size_)return false;
    if(s_[i_]=='"')return generalString();
    if(s_[i_]=='-'||digit(s_[i_]))return number(false,nullptr);
    if(s_[i_]=='n')return literal("null");
    if(s_[i_]=='t')return literal("true");
    if(s_[i_]=='f')return literal("false");
    return false; // nested objects/arrays are NOT current F06 journal schema
  }
  const char* s_;
  size_t size_,i_=0;
};
inline Verdict inspect(DTNJsonlSnapshot::Journal journal,uint32_t expectedVessel,
                       const char* json,size_t length,Fields& fields){
  fields=Fields{};
  if(expectedVessel<1U||expectedVessel>9999U ||
     !DTNJsonlSnapshot::filename(journal))return Verdict::UnsupportedInput;
  bool boot=false,index=false,up=false;
  Parser p(json,length);
  if(!p.parse(fields,boot,index,up))return Verdict::InvalidJson;
  if(fields.vesselId<1U||fields.vesselId>9999U)return Verdict::InvalidFields;
  if(fields.vesselId!=expectedVessel)return Verdict::WrongVessel;
  if(journal==DTNJsonlSnapshot::Journal::Diagnostics)return Verdict::DiagnosticsOnly;
  if(!boot||!index||!up||fields.eventIndex==0U)return Verdict::InvalidFields;
  return Verdict::CandidateUnverified; // NOT a backend-accepted record
}
// A local diagnostic locator only; NOT a UUID, authenticated ID, or
// backend idempotency key. Must refer to a closed, immutable journal.
inline bool localLocator(uint32_t vessel,const char* session,
                         DTNJsonlSnapshot::Journal journal,
                         const DTNJsonlSnapshot::Record& record,
                         char* out,size_t capacity){
  if(!out||capacity==0U||!DTNJsonlSnapshot::validSession(vessel,session)||
     !DTNJsonlSnapshot::filename(journal)||record.end<=record.start)return false;
  const int n=snprintf(out,capacity,"V%04lu/%s/%s#%llu@%llu-%llu:%08lX",
      static_cast<unsigned long>(vessel),session,
      DTNJsonlSnapshot::filename(journal),
      static_cast<unsigned long long>(record.ordinal),
      static_cast<unsigned long long>(record.start),
      static_cast<unsigned long long>(record.end),
      static_cast<unsigned long>(record.fnv1a));
  return n>0 && static_cast<size_t>(n)<capacity;
}
} // namespace DTNF06RecordGate
