#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cassert>
#define GRASS_CAPACITY_TEST_STATUS 1
#include "../cpu_patch.h"
extern "C" UINT invoke(void*,void*,void*,void*,void*,UINT,UINT,UINT);
static void putf(unsigned char*r,size_t at,float x){memcpy(r+at,&x,4);}
int main(int argc,char**argv){assert(argc==2);using namespace grassAlignedCapacity;std::ifstream in(argv[1],std::ios::binary);std::vector<unsigned char>b((std::istreambuf_iterator<char>(in)),{});auto*dos=(IMAGE_DOS_HEADER*)b.data();auto*nt=(IMAGE_NT_HEADERS64*)(b.data()+dos->e_lfanew);assert(nt->OptionalHeader.SizeOfImage==kImageSize);
 auto*image=(unsigned char*)VirtualAlloc(nullptr,kImageSize,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(image);memcpy(image,b.data(),nt->OptionalHeader.SizeOfHeaders);auto*sec=IMAGE_FIRST_SECTION(nt);for(UINT i=0;i<nt->FileHeader.NumberOfSections;++i)memcpy(image+sec[i].VirtualAddress,b.data()+sec[i].PointerToRawData,sec[i].SizeOfRawData);assert(!validateImage(image));
 unsigned char originalSquare[12];memcpy(originalSquare,image+kSquareRva[0],12);image[kFarRva]^=1;auto bad=redirectAlignedSquares(image);assert(!strcmp(bad.status,"far-instruction-mismatch"));assert(!memcmp(originalSquare,image+kSquareRva[0],12));image[kFarRva]^=1;
 // Stop immediately after the patched branch; no game function is executed.
 image[kFarRva+11]=0xc3;auto installed=redirectAlignedSquares(image);assert(!strcmp(installed.status,"applied-aligned-grid-and-far-reserve"));
 unsigned char renderer[0xc00]={},noise[0x100]={},root[0x40]={},settings[0x80]={},nodes[8][0x40]={};renderer[0x5f9]=1;UINT width=64;memcpy(noise+0xa4,&width,4);void*noisePtr=noise;memcpy(renderer+0x5a0,&noisePtr,8);putf(renderer,0xad8,160);putf(renderer,0xad4,1);putf(renderer,0xae4,.5f);putf(renderer,0xae8,1);putf(renderer,0xaec,2);putf(renderer,0xaf0,4); // grids 320 and 1280
 void*settingsPtr=settings;memcpy(root+0x18,&settingsPtr,8);auto*sentinel=settings+0x20;
 for(UINT i=0;i<8;++i){void*next=i==7?(void*)sentinel:(void*)nodes[i+1];memcpy(nodes[i]+8,&next,8);UINT cap=i<2?UINT_MAX:4096;memcpy(nodes[i]+0x28,&cap,4);}
 UINT metadata[8]={12,0,19584,0,6,0x11223344,0xdeadbeef,0x55667788};UINT*meta=metadata+4;auto run=[&](UINT index,UINT elements,UINT expected){meta[0]=elements;meta[2]=0xdeadbeef;UINT actual=invoke(image+kFarRva,renderer,meta,root,nodes[1],index,97920,19584);assert(actual==expected);assert(meta[1]==0x11223344&&meta[3]==0x55667788);};
 run(0,12,19584);run(1,6,1740800);run(2,6,78336);run(1,12,78336);
 auto status=getFarStatus();assert(status.capacity==1740800&&status.totalBytes==199915520&&status.otherCapacity==24576);
 ID3D11Device*d=nullptr;ID3D11DeviceContext*c=nullptr;D3D_FEATURE_LEVEL level;assert(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c)));
 D3D11_BUFFER_DESC bd={};bd.ByteWidth=128;bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_SHADER_RESOURCE;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;bd.StructureByteStride=16;ID3D11Buffer*mappedBuffer=nullptr;assert(SUCCEEDED(d->CreateBuffer(&bd,nullptr,&mappedBuffer)));D3D11_MAPPED_SUBRESOURCE mapped={};assert(SUCCEEDED(c->Map(mappedBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)));memcpy(mapped.pData,metadata,sizeof(metadata));auto*mappedMeta=(UINT*)mapped.pData+4;mappedMeta[0]=6;bool queryVisible=readable((unsigned char*)mappedMeta-8,12);UINT mappedResult=invoke(image+kFarRva,renderer,mappedMeta,root,nodes[1],1,97920,19584);assert(mappedResult==1740800);assert(mappedMeta[1]==0x11223344&&mappedMeta[3]==0x55667788);printf("PASS native D3DMetal mapped metadata: VirtualQuery_visible=%u capacity=%u\n",queryVisible?1:0,mappedResult);c->Unmap(mappedBuffer,0);mappedBuffer->Release();c->Release();d->Release();
 renderer[0x5f9]=0;run(1,6,78336);assert(!strcmp(getFarStatus().status,"generation-grid-not-ready"));renderer[0x5f9]=1;
 void*badNext=nodes[1];memcpy(nodes[1]+8,&badNext,8);run(1,6,78336);assert(!strcmp(getFarStatus().status,"invalid-material-tail"));
 printf("PASS actual EXE instruction signatures, atomic three-site install, Win64 branch execution, near/custom isolation, metadata canaries, unavailable-grid and invalid-tail fallbacks\n");return 0;
}
