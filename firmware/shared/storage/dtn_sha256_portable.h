#pragma once
// F24D: bounded streaming SHA-256 (FIPS 180-4), fixed memory, no SD or Wi-Fi.
// Integrity digest, not a signature or authenticated source identity.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace DTNSha256 {
class Hasher {
public:
  Hasher(){begin();}
  bool begin(){
    const uint32_t init[8]={0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
      0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    memcpy(h_,init,sizeof(init));used_=0U;total_=0U;ended_=false;
    memset(block_,0,sizeof(block_));return true;
  }
  bool update(const uint8_t* bytes,size_t length){
    if(ended_||(!bytes&&length)||UINT64_MAX-total_<length)return false;
    total_+=length;
    for(size_t i=0;i<length;++i){
      block_[used_++]=bytes[i];
      if(used_==64U){compress();used_=0U;}
    }
    return true;
  }
  bool end(uint8_t out[32]){
    if(ended_||!out||total_>UINT64_MAX/8U)return false;
    const uint64_t bits=total_*8U;
    block_[used_++]=0x80U;
    if(used_>56U){while(used_<64U)block_[used_++]=0U;compress();used_=0U;}
    while(used_<56U)block_[used_++]=0U;
    for(unsigned i=0;i<8U;++i)block_[56U+i]=static_cast<uint8_t>(bits>>(56U-8U*i));
    compress();
    for(unsigned i=0;i<8U;++i)for(unsigned j=0;j<4U;++j)
      out[4U*i+j]=static_cast<uint8_t>(h_[i]>>(24U-8U*j));
    ended_=true;memset(block_,0,sizeof(block_));return true;
  }
private:
  static uint32_t rr(uint32_t v,unsigned n){return (v>>n)|(v<<(32U-n));}
  void compress(){
    static constexpr uint32_t k[64]={
      0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
      0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
      0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
      0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
      0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
      0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
      0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
      0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
    uint32_t w[64]{};
    for(unsigned i=0;i<16U;++i){const unsigned p=4U*i;
      w[i]=(uint32_t(block_[p])<<24U)|(uint32_t(block_[p+1U])<<16U)|
           (uint32_t(block_[p+2U])<<8U)|uint32_t(block_[p+3U]);}
    for(unsigned i=16U;i<64U;++i){
      const uint32_t a=w[i-15U],b=w[i-2U];
      w[i]=w[i-16U]+(rr(a,7U)^rr(a,18U)^(a>>3U))+w[i-7U]+
           (rr(b,17U)^rr(b,19U)^(b>>10U));}
    uint32_t a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],z=h_[7];
    for(unsigned i=0;i<64U;++i){
      const uint32_t t1=z+(rr(e,6U)^rr(e,11U)^rr(e,25U))+((e&f)^((~e)&g))+k[i]+w[i];
      const uint32_t t2=(rr(a,2U)^rr(a,13U)^rr(a,22U))+((a&b)^(a&c)^(b&c));
      z=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h_[0]+=a;h_[1]+=b;h_[2]+=c;h_[3]+=d;
    h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=z;
  }
  uint32_t h_[8]{};uint8_t block_[64]{};
  size_t used_=0U;uint64_t total_=0U;bool ended_=false;
};
inline bool equals32(const uint8_t a[32],const uint8_t b[32]){
  if(!a||!b)return false;
  uint8_t difference=0U;
  for(unsigned i=0;i<32U;++i)difference|=static_cast<uint8_t>(a[i]^b[i]);
  return difference==0U;
}
} // DTNSha256
