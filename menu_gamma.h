// Exact DiRT UI shader correction. No game bytecode is embedded.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
extern "C" void vkd3d_compute_md5(const void*,size_t,uint32_t[4],int);
namespace dirtMenuGamma {
enum class Result {NoMatch,Patched,Rejected};
static uint32_t read(const unsigned char*p){uint32_t x;memcpy(&x,p,4);return x;}
static void write(unsigned char*p,uint32_t x){memcpy(p,&x,4);}
static uint32_t crc32(const unsigned char*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
// The UI hashes and unwanted 1/2.2 exponent are documented in RenoDX's
// dirtrally2 UI1 (289F2576) and UI2 (384FC9E6). Keep the game's code,
// registers, bindings, opacity, user gamma and blending intact.
static bool editCode(unsigned char*code,size_t size){
 if(size<8 || size%4 || read(code)!=0x50 || read(code+4)!=size/4)return false;
 size_t at=2,changed=0;
 while(at<size/4){
  uint32_t token=read(code+at*4);size_t count=(token>>24)&127;
  if((token&2047)==53){if(at+1>=size/4)return false;count=read(code+(at+1)*4);}
  if(!count || count>size/4-at)return false;
  if((token&2047)==56){ // MUL instruction, never declarations or metadata
   for(size_t j=at+1;j<at+count;++j){
    uint32_t operand=read(code+j*4);
    if(operand==0x00004001 && j+1<at+count && read(code+(j+1)*4)==0x3ee8ba2f){write(code+(j+1)*4,0x3f800000);++changed;++j;}
    else if(operand==0x00004002 && j+4<at+count && read(code+(j+1)*4)==0x3ee8ba2f && read(code+(j+2)*4)==0x3ee8ba2f && read(code+(j+3)*4)==0x3ee8ba2f){
     auto last=read(code+(j+4)*4);if(last!=0 && last!=0x3ee8ba2f)return false;
     for(size_t k=1;k<=4;++k)if(read(code+(j+k)*4)==0x3ee8ba2f)write(code+(j+k)*4,0x3f800000);
     ++changed;j+=4;
    }
   }
  }
  at+=count;
 }
 return changed==1;
}
static Result patch(const void*raw,size_t n,std::vector<unsigned char>&out,uint32_t*identity=nullptr){
 out.clear();if(!raw||n<32||n>1024*1024)return Result::NoMatch;
 auto*b=static_cast<const unsigned char*>(raw);if(memcmp(b,"DXBC",4))return Result::NoMatch;
 auto hash=crc32(b,n);if(identity)*identity=hash;
 if(hash!=0x289f2576 && hash!=0x384fc9e6)return Result::NoMatch;
 if(read(b+20)!=1 || read(b+24)!=n)return Result::Rejected;
 uint32_t digest[4];vkd3d_compute_md5(b+20,n-20,digest,1);if(memcmp(b+4,digest,16))return Result::Rejected;
 size_t chunks=read(b+28);if(chunks>(n-32)/4)return Result::Rejected;
 out.assign(b,b+n);size_t programs=0;
 for(size_t i=0;i<chunks;++i){
  size_t at=read(b+32+4*i);if(at<32+chunks*4||at>n-8)return Result::Rejected;
  size_t length=read(b+at+4);if(length>n-at-8)return Result::Rejected;
  if(!memcmp(b+at,"SHEX",4)||!memcmp(b+at,"SHDR",4)){
   ++programs;if(!editCode(out.data()+at+8,length)){out.clear();return Result::Rejected;}
  }
 }
 if(programs!=1){out.clear();return Result::Rejected;}
 vkd3d_compute_md5(out.data()+20,n-20,digest,1);memcpy(out.data()+4,digest,16);return Result::Patched;
}
}
