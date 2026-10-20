// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 armax9
#pragma once
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <cmath>

// Called synchronously from DLL_PROCESS_ATTACH, before the game's renderer starts.
// Only Kernel32 APIs and small CRT memory operations are used here.
namespace grassAlignedCapacity {
static constexpr DWORD kTimestamp=0x605cbae3, kImageSize=0x2255000;
static constexpr DWORD kLoadRva=0x9f96a4, kSignatureRva=0x9f9695;
static constexpr DWORD kScalarRva=0x139d61c, kFileSize=24668160;
static constexpr uint32_t kOriginalBits=0x3de66666;
static const unsigned char kLoad[9]={0xf3,0x44,0x0f,0x10,0x0d,0x6f,0x3f,0x9a,0x00};
static const unsigned char kSignature[42]={
 0xf3,0x44,0x0f,0x10,0x05,0x2a,0xe6,0x7f,0x00,0x44,0x0f,0x29,0x4c,0x24,0x40,
 0xf3,0x44,0x0f,0x10,0x0d,0x6f,0x3f,0x9a,0x00,0x0f,0x29,0x74,0x24,0x70,
 0x0f,0x29,0x7c,0x24,0x60,0x44,0x89,0xac,0x24,0xe0,0x00,0x00,0x00};
struct Result {const char*status="not-requested";void*trampoline=nullptr;int32_t displacement[2]={};};

static bool filenameIsGame(const wchar_t*p){
 const wchar_t*tail=wcsrchr(p,L'\\');tail=tail?tail+1:p;
 const wchar_t expected[]=L"dirtrally2.exe";
 for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);++i){
  wchar_t c=tail[i];if(c>=L'A'&&c<=L'Z')c+=L'a'-L'A';
  if(c!=expected[i])return false;if(!c)return true;
 }return false;
}
static bool inSection(IMAGE_NT_HEADERS64*nt,DWORD rva,DWORD bytes,DWORD required){
 auto*s=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){
  uint64_t start=s[i].VirtualAddress,end=start+s[i].Misc.VirtualSize;
  if(rva>=start&&(uint64_t)rva+bytes<=end&&(s[i].Characteristics&required)==required)return true;
 }return false;
}
static const char*validateImage(unsigned char*base){
 if(!base)return "no-main-image";
 auto*dos=(IMAGE_DOS_HEADER*)base;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<64||dos->e_lfanew>0x1000)return "invalid-dos-header";
 auto*nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return "invalid-pe64-header";
 if(nt->FileHeader.TimeDateStamp!=kTimestamp||nt->OptionalHeader.SizeOfImage!=kImageSize||nt->FileHeader.NumberOfSections>32||nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64))return "unsupported-pe-version";
 if(!inSection(nt,kSignatureRva,sizeof(kSignature),IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_EXECUTE)||!inSection(nt,kScalarRva,4,IMAGE_SCN_MEM_READ))return "invalid-code-or-data-section";
 if(memcmp(base+kSignatureRva,kSignature,sizeof(kSignature)))return "instruction-signature-mismatch";
 uint32_t bits=0;memcpy(&bits,base+kScalarRva,4);if(bits!=kOriginalBits)return "original-scalar-mismatch";
 int32_t disp=0;memcpy(&disp,base+kLoadRva+5,4);
 if(base+kLoadRva+9+(intptr_t)disp!=base+kScalarRva)return "original-rip-target-mismatch";
 return nullptr;
}
static bool displacementFor(uintptr_t next,uintptr_t target,int32_t*out){
 int64_t d=(int64_t)target-(int64_t)next;
 if(d<INT32_MIN||d>INT32_MAX)return false;*out=(int32_t)d;return true;
}
static void*allocateRange(uintptr_t low,uintptr_t high,uintptr_t next,DWORD granularity,DWORD pageSize){
 uintptr_t at=(low+granularity-1)&~(uintptr_t)(granularity-1);
 while(at<=high&&high-at>=pageSize-1){
  MEMORY_BASIC_INFORMATION mb={};if(!VirtualQuery((void*)at,&mb,sizeof(mb)))break;
  uintptr_t regionEnd=(uintptr_t)mb.BaseAddress+mb.RegionSize;if(regionEnd<=at)break;
  if(mb.State==MEM_FREE){
   uintptr_t candidate=(at+granularity-1)&~(uintptr_t)(granularity-1);int32_t d;
   if(candidate<=high&&high-candidate>=pageSize-1&&candidate+pageSize<=regionEnd&&displacementFor(next,candidate,&d)){
    void*p=VirtualAlloc((void*)candidate,pageSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(p){if(p==(void*)candidate)return p;VirtualFree(p,0,MEM_RELEASE);}
   }
  }
  if(regionEnd>UINTPTR_MAX-granularity)break;
  at=(regionEnd+granularity-1)&~(uintptr_t)(granularity-1);
 }return nullptr;
}

// Reproduce the generation-target calculation at EXE0x1409F4F03..4FC0.
// Read the actual noise-texture width, not an assumed constant64 or uninitialized
// per-frame dimensions at renderer+0x5f0. Unsupported/unready inputs use the
// original allocation formula and retain the independent shader bounds guard.
static float readFloat(const unsigned char*r,size_t off){float f;memcpy(&f,r+off,4);return f;}
static float originalDimension(const unsigned char*r,unsigned cascade){
 float n=readFloat(r,0xad8)*2.0f;
 n=n*readFloat(r,0xae4+4*cascade);
 n=n*readFloat(r,0xaec+4*cascade);
 n=n*readFloat(r,0xad4);
 return std::ceil(n);
}
static bool readable(const void*p,SIZE_T bytes){
 MEMORY_BASIC_INFORMATION m={};
 if(!VirtualQuery(p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
 uintptr_t a=(uintptr_t)p,start=(uintptr_t)m.BaseAddress;
 return a>=start&&bytes<=m.RegionSize&&a-start<=m.RegionSize-bytes;
}
struct DimensionResult{float original=0,effective=0;DWORD width=0,ready=0,reason=0;bool used=false;};
static DimensionResult calculateDimension(const unsigned char*r,unsigned cascade){
 DimensionResult result;
 if(cascade>1){result.reason=8;return result;}
 result.original=originalDimension(r,cascade);result.effective=result.original;
 result.ready=r[0x5f9];
 if(result.ready!=1){result.reason=1;return result;}
 float d=readFloat(r,0xad8),c=readFloat(r,0xae4+4*cascade);
 float scale=readFloat(r,0xaec+4*cascade),lod=readFloat(r,0xad4);
 if(!std::isfinite(d)||!std::isfinite(c)||!std::isfinite(scale)||!std::isfinite(lod)||d<=0||c<=0||scale<=0||lod<=0){result.reason=5;return result;}
 uintptr_t texture=0;memcpy(&texture,r+0x5a0,sizeof(texture));
 if(!texture){result.reason=2;return result;}
 if(texture>UINTPTR_MAX-0xa8||!readable((void*)(texture+0xa4),4)){result.reason=3;return result;}
 uint32_t alignment=0;memcpy(&alignment,(void*)(texture+0xa4),4);
 result.width=alignment;
 if(!alignment||alignment>16384||(alignment&(alignment-1))){result.reason=4;return result;}
 float radius=std::ceil(d*c);
 float nominal=radius*2.0f;nominal=nominal*scale;nominal=nominal*lod;
 nominal=std::ceil(nominal);
 if(!std::isfinite(nominal)||nominal<0||nominal>16384){result.reason=6;return result;}
 uint32_t n=(uint32_t)nominal;if(n<1)n=1;
 uint32_t rounded=(n+alignment-1)&~(alignment-1);
 if(!rounded||rounded>16384){result.reason=7;return result;}
 result.used=true;result.effective=(float)rounded;return result;
}
static float generationDimension(const unsigned char*r,unsigned cascade,bool*aligned=nullptr){auto x=calculateDimension(r,cascade);if(aligned)*aligned=x.used;return x.effective;}
static float alignedSquare0(const unsigned char*r){auto x=calculateDimension(r,0);return x.effective*x.effective;}
static float alignedSquare1(const unsigned char*r){auto x=calculateDimension(r,1);return x.effective*x.effective;}
static constexpr DWORD kSquareRva[2]={0x9f9836,0x9f97b7};
static const unsigned char kSquareInstructions[2][12]={
 {0x41,0x0f,0x28,0xc8,0x0f,0x28,0xc6,0xe8,0xee,0xa1,0x4d,0x00},
 {0x41,0x0f,0x28,0xc8,0x0f,0x28,0xc2,0xe8,0x6d,0xa2,0x4d,0x00}};

static Result redirectAlignedSquares(unsigned char*base){
 Result result;
 for(unsigned i=0;i<2;++i)if(memcmp(base+kSquareRva[i],kSquareInstructions[i],12)){result.status="square-instruction-mismatch";return result;}
 SYSTEM_INFO si={};GetSystemInfo(&si);
 uintptr_t next=(uintptr_t)base+kSquareRva[0]+8;
 uintptr_t low=next>(uintptr_t)INT32_MAX?next-(uintptr_t)INT32_MAX:0;
 uintptr_t high=next+(uintptr_t)INT32_MAX;
 if(low<(uintptr_t)si.lpMinimumApplicationAddress)low=(uintptr_t)si.lpMinimumApplicationAddress;
 if(high>(uintptr_t)si.lpMaximumApplicationAddress)high=(uintptr_t)si.lpMaximumApplicationAddress;
 uintptr_t split=(next+si.dwAllocationGranularity-1)&~(uintptr_t)(si.dwAllocationGranularity-1);
 auto*page=(unsigned char*)allocateRange(split,high,next,si.dwAllocationGranularity,si.dwPageSize);
 if(!page&&split>low)page=(unsigned char*)allocateRange(low,split-1,next,si.dwAllocationGranularity,si.dwPageSize);
 if(!page){result.status="no-near-trampoline";return result;}
 auto fail=[&](const char*s){VirtualFree(page,0,MEM_RELEASE);Result r;r.status=s;return r;};
 const void*helpers[2]={(const void*)alignedSquare0,(const void*)alignedSquare1};
 unsigned char patches[2][12];
 for(unsigned i=0;i<2;++i){
  auto*stub=page+16*i;
  // mov rax,absolute_helper; jmp rax. RAX is volatile in the original POW call.
  stub[0]=0x48;stub[1]=0xb8;memcpy(stub+2,&helpers[i],8);stub[10]=0xff;stub[11]=0xe0;
  // mov rcx,rdi; call near_stub; nop*4. Original MS-x64 shadow space,
  // alignment and return address are retained; helper preserves nonvolatiles.
  const unsigned char shape[12]={0x48,0x8b,0xcf,0xe8,0,0,0,0,0x90,0x90,0x90,0x90};
  memcpy(patches[i],shape,12);
  if(!displacementFor((uintptr_t)base+kSquareRva[i]+8,(uintptr_t)stub,&result.displacement[i]))return fail("trampoline-out-of-rip-range");
  memcpy(patches[i]+4,&result.displacement[i],4);
 }
 DWORD ignored=0;
 if(!VirtualProtect(page,si.dwPageSize,PAGE_EXECUTE_READ,&ignored)||!FlushInstructionCache(GetCurrentProcess(),page,32))return fail("trampoline-protection-or-flush-failed");
 for(unsigned i=0;i<2;++i)if(memcmp(base+kSquareRva[i],kSquareInstructions[i],12))return fail("square-code-changed-before-patch");
 // Both exact supported sites lie within the same code page. Protect/commit
 // together and restore both original instruction blocks on any failure.
 uintptr_t page0=((uintptr_t)base+kSquareRva[0])&~(uintptr_t)(si.dwPageSize-1);
 uintptr_t page1=((uintptr_t)base+kSquareRva[1])&~(uintptr_t)(si.dwPageSize-1);
 if(page0!=page1)return fail("unexpected-code-page-layout");
 DWORD old=0;if(!VirtualProtect((void*)page0,si.dwPageSize,PAGE_EXECUTE_READWRITE,&old))return fail("code-protection-failed");
 for(unsigned i=0;i<2;++i)memcpy(base+kSquareRva[i],patches[i],12);
 if(!FlushInstructionCache(GetCurrentProcess(),(void*)page0,si.dwPageSize)){
  for(unsigned i=0;i<2;++i)memcpy(base+kSquareRva[i],kSquareInstructions[i],12);
  FlushInstructionCache(GetCurrentProcess(),(void*)page0,si.dwPageSize);
  VirtualProtect((void*)page0,si.dwPageSize,old,&ignored);return fail("code-flush-failed-rolled-back");
 }
 if(!VirtualProtect((void*)page0,si.dwPageSize,old,&ignored)){
  for(unsigned i=0;i<2;++i)memcpy(base+kSquareRva[i],kSquareInstructions[i],12);
  FlushInstructionCache(GetCurrentProcess(),(void*)page0,si.dwPageSize);
  VirtualProtect((void*)page0,si.dwPageSize,old,&ignored);return fail("code-protection-restore-failed-rolled-back");
 }
 result.status="applied-aligned-grid";result.trampoline=page;return result;
}
static Result install(unsigned char*base,const wchar_t*exePath){
 Result r;if(!filenameIsGame(exePath)){r.status="not-dirtrally2-exe";return r;}
 WIN32_FILE_ATTRIBUTE_DATA fd={};
 if(!GetFileAttributesExW(exePath,GetFileExInfoStandard,&fd)||fd.nFileSizeHigh||fd.nFileSizeLow!=kFileSize){r.status="unsupported-exe-file-size";return r;}
 const char*invalid=validateImage(base);if(invalid){r.status=invalid;return r;}
 return redirectAlignedSquares(base);
}
}
