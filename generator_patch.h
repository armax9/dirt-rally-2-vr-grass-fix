// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 armax9
#pragma once
// Runtime edits for one exact game-supplied DXBC. Contains no complete shader.
// Unknown/corrupt inputs fail closed. The caller retains the original shader.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>
extern "C" { void vkd3d_compute_md5(const void*, size_t, uint32_t[4], int); }

namespace grassGenerator {
enum class Result { NoMatch, Patched, Rejected };
struct Info { const char* reason = "no-match"; };
static constexpr size_t kOriginalBytes = 12460;
static constexpr size_t kPatchedBytes = 12504;
static constexpr size_t kCodeOffset = 3460;
static constexpr size_t kOriginalWords = 2211;
static constexpr size_t kPatchedWords = 2222;
static const unsigned char kOriginalDigest[16] = {
  0x22,0xb8,0x0c,0x77,0x15,0x95,0xc2,0x0c,0x6a,0xc2,0x21,0x75,0xcf,0xf8,0x05,0xdc};
static const unsigned char kGuardedDigest[16] = {
  0x6a,0x4a,0xd0,0xd1,0x6e,0x96,0x3f,0x35,0xa9,0x82,0xa9,0x34,0x91,0x18,0xb6,0x65};
static const unsigned char kNoFrustumDigest[16] = {
  0x91,0x86,0x14,0x6d,0x11,0x0b,0x5e,0xc2,0x0f,0xf8,0xb5,0x4e,0xe0,0xa5,0x1a,0xdf};

static uint32_t read32(const unsigned char* p) {
  return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
static void write32(unsigned char* p, uint32_t v) {
  for (unsigned i=0;i<4;++i) p[i]=static_cast<unsigned char>(v>>(8*i));
}
static void digest(const unsigned char* p, size_t bytes, unsigned char out[16]) {
  uint32_t d[4]; vkd3d_compute_md5(p+20,bytes-20,d,1);
  for (unsigned i=0;i<4;++i) write32(out+4*i,d[i]);
}
static bool equalWords(const unsigned char* b, const uint32_t* words, size_t count) {
  for(size_t i=0;i<count;++i) if(read32(b+4*i)!=words[i]) return false;
  return true;
}
// Decode tokenized instructions, including custom data, to require edits on
// instruction boundaries rather than matching a byte pattern within operands.
static bool instructionBoundaries(const unsigned char* code, size_t words,
                                  size_t first, size_t second, size_t third) {
  bool seen[3]={false,false,false};
  size_t word=2;
  while(word<words) {
    if(word==first) seen[0]=true;
    if(word==second) seen[1]=true;
    if(word==third) seen[2]=true;
    const uint32_t token=read32(code+4*word);
    size_t length=(token>>24)&0x7f;
    if((token&0x7ff)==53) {
      if(word+1>=words) return false;
      length=read32(code+4*(word+1));
    }
    if(!length || length>words-word) return false;
    word+=length;
  }
  return word==words && seen[0] && seen[1] && seen[2];
}
static Result build(const void* data, size_t bytes, bool noFrustum,
                    std::vector<unsigned char>& output, Info* info=nullptr) {
  Info local; if(!info) info=&local; *info=Info{}; output.clear();
  if(!data || bytes!=kOriginalBytes) return Result::NoMatch;
  const auto* b=static_cast<const unsigned char*>(data);
  if(memcmp(b,"DXBC",4) || memcmp(b+4,kOriginalDigest,16)) return Result::NoMatch;
  unsigned char hash[16]; digest(b,bytes,hash);
  if(memcmp(hash,kOriginalDigest,16)) {
    info->reason="original-checksum-mismatch"; return Result::Rejected;
  }
  info->reason="original-container-mismatch";
  if(read32(b+20)!=1 || read32(b+24)!=bytes || read32(b+28)!=5) return Result::Rejected;
  static const uint32_t offsets[5]={52,3276,3436,3452,12304};
  static const uint32_t sizes[5]={3216,152,8,8844,148};
  static const char tags[5][5]={"RDEF","ISGN","OSGN","SHEX","STAT"};
  for(unsigned i=0;i<5;++i) {
    const uint32_t at=read32(b+32+4*i);
    if(at!=offsets[i] || memcmp(b+at,tags[i],4) || read32(b+at+4)!=sizes[i])
      return Result::Rejected;
  }
  const unsigned char* code=b+kCodeOffset;
  static constexpr size_t kGuardInsertion=0x2de8;
  static constexpr size_t kEndInsertion=0x2fd8;
  static constexpr size_t kClipPredicate=0x1100;
  if(read32(code)!=0x50 || read32(code+4)!=kOriginalWords ||
     !instructionBoundaries(code,kOriginalWords,
        (kGuardInsertion-kCodeOffset)/4,(kEndInsertion-kCodeOffset)/4,
        (kClipPredicate-kCodeOffset)/4)) {
    info->reason="original-instruction-stream-mismatch"; return Result::Rejected;
  }
  // Guard must follow this atomic increment, precede the remaining stores, and
  // close before the existing counter/loop tail. These are local context checks.
  static const uint32_t beforeGuard[9]={
    0x090000b4,0x00100012,8,0x0011e000,2,0x00100046,4,0x00004001,1};
  static const uint32_t afterGuard[11]={
    0x8b0000a7,0x80008302,0x00199983,0x00100082,0,0x0010000a,4,
    0x00004001,4,0x00107006,7};
  static const uint32_t afterEnd[5]={0x050000b2,0x00100012,0,0x0011e000,1};
  static const uint32_t oldClip[7]={0x0700003c,0x00100012,0,0x0010000a,0,0x0010000a,1};
  if(!equalWords(b+kGuardInsertion-sizeof(beforeGuard),beforeGuard,9) ||
     !equalWords(b+kGuardInsertion,afterGuard,11) ||
     !equalWords(b+kEndInsertion,afterEnd,5) ||
     !equalWords(b+kClipPredicate,oldClip,7)) {
    info->reason="original-edit-context-mismatch"; return Result::Rejected;
  }
  // ULT r0.w,r8.x,r2.w; IF_NZ r0.w. r8.x is the index returned by
  // the atomic increment; r2.w is the capacity. Existing precheck stays intact.
  static const uint32_t guard[10]={
    0x0700004f,0x00100082,0,0x0010000a,8,0x0010003a,2,
    0x0304001f,0x0010003a,0};
  output.reserve(kPatchedBytes);
  output.insert(output.end(),b,b+kGuardInsertion);
  for(uint32_t word:guard) {
    unsigned char encoded[4]; write32(encoded,word);
    output.insert(output.end(),encoded,encoded+4);
  }
  output.insert(output.end(),b+kGuardInsertion,b+kEndInsertion);
  unsigned char endIf[4]; write32(endIf,0x01000015);
  output.insert(output.end(),endIf,endIf+4);
  output.insert(output.end(),b+kEndInsertion,b+bytes);
  write32(output.data()+24,kPatchedBytes);
  write32(output.data()+48,12348);         // STAT offset follows the longer SHEX.
  write32(output.data()+3456,8888);        // SHEX byte count, +44 bytes.
  write32(output.data()+kCodeOffset+4,kPatchedWords);
  if(noFrustum) {
    // XOR r0.x,r0.x,r0.x produces zero only for the combined projection-plane
    // predicate. Subsequent slope/facing tests and all generation data remain.
    write32(output.data()+kClipPredicate,0x07000057);
    write32(output.data()+kClipPredicate+24,0);
  }
  digest(output.data(),output.size(),hash);
  const unsigned char* expected=noFrustum?kNoFrustumDigest:kGuardedDigest;
  if(memcmp(hash,expected,16)) {
    output.clear(); info->reason="output-checksum-mismatch"; return Result::Rejected;
  }
  memcpy(output.data()+4,hash,16);
  info->reason=noFrustum?"patched-generator-no-frustum":"patched-generator-guarded";
  return Result::Patched;
}
// The main working fix. The caller can separately buildGuarded() if creation of
// this no-frustum candidate fails, then forward the original if that also fails.
static Result buildNoFrustum(const void* data, size_t bytes,
                            std::vector<unsigned char>& output, Info* info=nullptr) {
  return build(data,bytes,true,output,info);
}
static Result buildGuarded(const void* data, size_t bytes,
                          std::vector<unsigned char>& output, Info* info=nullptr) {
  return build(data,bytes,false,output,info);
}
static Result patch(const void* data, size_t bytes, std::vector<unsigned char>& output,
                    bool noFrustum=true, Info* info=nullptr) {
  return build(data,bytes,noFrustum,output,info);
}
} // namespace grassGenerator
