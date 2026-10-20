// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 armax9
#pragma once
// UV-anchored fade for one exact grass depth/fade PS. No complete game
// shader is embedded: the target supplied by the game is validated and edited.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>
extern "C" { void vkd3d_compute_md5(const void*,size_t,uint32_t[4],int); }
namespace grassUVFade {
enum class Result { NoMatch, Patched, Rejected };
struct Info { const char* reason="no-match"; };
static const unsigned char originalDigest[16]={0x19,0x50,0x6c,0x9c,0xc8,0x40,0x93,0x93,0x08,0xf1,0x51,0xd9,0xee,0x36,0xd8,0x1c};
static const unsigned char candidateDigest[16]={0xae,0xcc,0xdb,0xef,0x9b,0x82,0x1f,0xbc,0xab,0x48,0x0c,0xc3,0x4b,0x45,0x77,0xc5};
static uint32_t read32(const unsigned char* p){uint32_t v;memcpy(&v,p,4);return v;}
static void write32(unsigned char* p,uint32_t v){memcpy(p,&v,4);}
static void digest(const unsigned char* p,size_t bytes,unsigned char out[16]){
 uint32_t d[4];vkd3d_compute_md5(p+20,bytes-20,d,1);memcpy(out,d,16);
}
static Result patch(const void* data,size_t bytes,std::vector<unsigned char>& output,Info* info=nullptr){
 Info local;if(!info)info=&local;*info=Info{};output.clear();
 if(!data||bytes!=3204)return Result::NoMatch;
 const auto* b=(const unsigned char*)data;
 if(memcmp(b,"DXBC",4)||memcmp(b+4,originalDigest,16))return Result::NoMatch;
 info->reason="invalid-container";
 if(read32(b+20)!=1||read32(b+24)!=bytes||read32(b+28)!=5)return Result::Rejected;
 unsigned char hash[16];digest(b,bytes,hash);
 if(memcmp(hash,originalDigest,16)){info->reason="original-checksum-mismatch";return Result::Rejected;}
 // Require the exact original five-chunk schema before inspecting token words.
 static const uint32_t offsets[5]={52,516,628,680,3048};
 static const uint32_t sizes[5]={456,104,44,2360,148};
 static const char tags[5][5]={"RDEF","ISGN","OSGN","SHEX","STAT"};
 for(unsigned i=0;i<5;++i){
  uint32_t at=read32(b+32+4*i);
  if(at!=offsets[i]||memcmp(b+at,tags[i],4)||read32(b+at+4)!=sizes[i])return Result::Rejected;
 }
 const unsigned char* code=b+688;
 if(read32(code)!=0x50||read32(code+4)!=590){info->reason="ps-version-or-length-mismatch";return Result::Rejected;}
 // Verify that word287 is an instruction boundary, then the exact original op.
 size_t word=2;bool targetBoundary=false;
 while(word<590){
  if(word==287)targetBoundary=true;
  uint32_t token=read32(code+4*word);size_t count=(token>>24)&0x7f;
  if((token&0x7ff)==53){if(word+1>=590)return Result::Rejected;count=read32(code+4*(word+1));}
  if(!count||count>590-word){info->reason="invalid-instruction-stream";return Result::Rejected;}
  word+=count;
 }
 static const uint32_t oldIndex[5]={0x0500001b,0x00100032,0,0x00101046,0};
 if(word!=590||!targetBoundary||memcmp(code+4*287,oldIndex,sizeof(oldIndex))){info->reason="index-instruction-mismatch";return Result::Rejected;}
 const unsigned char* stat=b+3056;
 if(read32(stat)!=40||read32(stat+16)!=27||read32(stat+84)!=3){info->reason="statistics-mismatch";return Result::Rejected;}
 // resinfo t0 mip0 dimensions; multiply UV; floor; integer Bayer coordinates.
 static const uint32_t indexUV[26]={
  0x8900003d,0x800000c2,0x00155543,0x00100032,0,0x00004001,0,0x00107e46,0,
  0x07000038,0x00100032,0,0x00100046,0,0x00101046,1,
  0x05000041,0x00100032,0,0x00100046,0,
  0x0500001b,0x00100032,0,0x00100046,0};
 output.assign(b,b+52);
 for(unsigned i=0;i<5;++i){
  size_t start=output.size();write32(output.data()+32+4*i,(uint32_t)start);
  const unsigned char* chunk=b+offsets[i];
  if(i==3){
   output.insert(output.end(),chunk,chunk+8+4*287);
   output.insert(output.end(),(const unsigned char*)indexUV,(const unsigned char*)indexUV+sizeof(indexUV));
   output.insert(output.end(),code+4*292,code+2360);
   write32(output.data()+start+4,2444);write32(output.data()+start+12,611);
  }else{
   output.insert(output.end(),chunk,chunk+8+sizes[i]);
   if(i==4){write32(output.data()+start+8,43);write32(output.data()+start+24,29);write32(output.data()+start+92,4);}
  }
 }
 if(output.size()!=3288){output.clear();info->reason="candidate-length-mismatch";return Result::Rejected;}
 write32(output.data()+24,(uint32_t)output.size());digest(output.data(),output.size(),hash);
 if(memcmp(hash,candidateDigest,16)){output.clear();info->reason="candidate-checksum-mismatch";return Result::Rejected;}
 memcpy(output.data()+4,hash,16);info->reason="patched-uv-anchored-bayer";return Result::Patched;
}
}
