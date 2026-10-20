// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 armax9
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "cpu_patch.h"
#include "generator_patch.h"
#include "vs_patch.h"
#include "uv_fade.h"

// Original D3D11 COM objects and shader fallback behavior are retained.
static INIT_ONCE versionOnce=INIT_ONCE_STATIC_INIT;
static SRWLOCK lock=SRWLOCK_INIT;
static HMODULE versionModule;
using CreateDeviceFn=decltype(&D3D11CreateDevice);
using CreatePSFn=HRESULT (STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11PixelShader**);
using CreateVSFn=HRESULT (STDMETHODCALLTYPE*)(ID3D11Device*,const void*,SIZE_T,ID3D11ClassLinkage*,ID3D11VertexShader**);
static CreateDeviceFn originalCreateDevice;
static CreatePSFn originalCreatePS;
static CreateVSFn originalCreateVS;

static HRESULT STDMETHODCALLTYPE captureVS(ID3D11Device* dev,const void* data,SIZE_T len,ID3D11ClassLinkage* linkage,ID3D11VertexShader** result) {
 std::vector<unsigned char> patched;
 if(grassVSInvariance::patch(data,len,patched)==grassVSInvariance::Result::Patched){
  HRESULT hr=originalCreateVS(dev,patched.data(),patched.size(),linkage,result);
  if(SUCCEEDED(hr))return hr;
 }
 return originalCreateVS(dev,data,len,linkage,result);
}

static HRESULT STDMETHODCALLTYPE capturePS(ID3D11Device* dev,const void* data,SIZE_T len,ID3D11ClassLinkage* linkage,ID3D11PixelShader** result) {
 std::vector<unsigned char> patched;
 if(grassUVFade::patch(data,len,patched)==grassUVFade::Result::Patched){
  HRESULT hr=originalCreatePS(dev,patched.data(),patched.size(),linkage,result);
  if(SUCCEEDED(hr))return hr;
 }
 // If the camera edit or its shader creation is rejected, retain the bounds
 // guard alone; if that also fails, submit the game's original shader.
 if(grassGenerator::patch(data,len,patched,true)==grassGenerator::Result::Patched){
  HRESULT hr=originalCreatePS(dev,patched.data(),patched.size(),linkage,result);
  if(SUCCEEDED(hr))return hr;
 }
 if(grassGenerator::patch(data,len,patched,false)==grassGenerator::Result::Patched){
  HRESULT hr=originalCreatePS(dev,patched.data(),patched.size(),linkage,result);
  if(SUCCEEDED(hr))return hr;
 }
 return originalCreatePS(dev,data,len,linkage,result);
}

static bool swapPointer(void** slot,void* value,void** previous) {
 DWORD old;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return false;
 if(previous)*previous=*slot;InterlockedExchangePointer(slot,value);DWORD ignored;VirtualProtect(slot,sizeof(void*),old,&ignored);return true;
}
static HRESULT WINAPI createDevice(IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT num,UINT sdk,ID3D11Device** dev,D3D_FEATURE_LEVEL* outlevel,ID3D11DeviceContext** ctx) {
 HRESULT hr=originalCreateDevice(adapter,type,software,flags,levels,num,sdk,dev,outlevel,ctx);
 if(SUCCEEDED(hr)&&dev&&*dev){
  auto table=*(void***)*dev;
  AcquireSRWLockExclusive(&lock);
  bool hooked=table[15]==(void*)capturePS;
  if(!hooked){if(!originalCreatePS)originalCreatePS=(CreatePSFn)table[15];if(table[15]==(void*)originalCreatePS)hooked=swapPointer(&table[15],(void*)capturePS,nullptr);}
  bool vertexHooked=table[12]==(void*)captureVS;
  if(!vertexHooked){if(!originalCreateVS)originalCreateVS=(CreateVSFn)table[12];if(table[12]==(void*)originalCreateVS)vertexHooked=swapPointer(&table[12],(void*)captureVS,nullptr);}
  ReleaseSRWLockExclusive(&lock);
 }
 return hr;
}
static void installIAT() {
 auto base=(unsigned char*)GetModuleHandleW(nullptr);auto dos=(IMAGE_DOS_HEADER*)base;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return;
 auto nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);
 auto imports=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!imports.VirtualAddress)return;
 for(auto d=(IMAGE_IMPORT_DESCRIPTOR*)(base+imports.VirtualAddress);d->Name;++d){
  if(_stricmp((const char*)(base+d->Name),"d3d11.dll")||!d->OriginalFirstThunk)continue;
  auto names=(IMAGE_THUNK_DATA64*)(base+d->OriginalFirstThunk);auto iat=(IMAGE_THUNK_DATA64*)(base+d->FirstThunk);
  for(;names->u1.AddressOfData;++names,++iat){
   if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;
   auto name=(IMAGE_IMPORT_BY_NAME*)(base+names->u1.AddressOfData);
   if(!strcmp((const char*)name->Name,"D3D11CreateDevice"))swapPointer((void**)&iat->u1.Function,(void*)createDevice,(void**)&originalCreateDevice);
  }
 }
}
static BOOL CALLBACK initVersion(PINIT_ONCE,void*,void**) {
 wchar_t path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);wcscat_s(path,L"\\version.dll");versionModule=LoadLibraryW(path);return versionModule!=nullptr;
}
extern "C" BOOL WINAPI GetFileVersionInfoA(LPCSTR file,DWORD handle,DWORD len,LPVOID data){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&GetFileVersionInfoA))GetProcAddress(versionModule,"GetFileVersionInfoA"):nullptr;return fn?fn(file,handle,len,data):FALSE;}
extern "C" BOOL WINAPI GetFileVersionInfoExA(DWORD flags,LPCSTR file,DWORD handle,DWORD len,LPVOID data){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&GetFileVersionInfoExA))GetProcAddress(versionModule,"GetFileVersionInfoExA"):nullptr;return fn?fn(flags,file,handle,len,data):FALSE;}
extern "C" DWORD WINAPI GetFileVersionInfoSizeA(LPCSTR file,LPDWORD handle){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&GetFileVersionInfoSizeA))GetProcAddress(versionModule,"GetFileVersionInfoSizeA"):nullptr;return fn?fn(file,handle):0;}
extern "C" DWORD WINAPI GetFileVersionInfoSizeExA(DWORD flags,LPCSTR file,LPDWORD handle){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&GetFileVersionInfoSizeExA))GetProcAddress(versionModule,"GetFileVersionInfoSizeExA"):nullptr;return fn?fn(flags,file,handle):0;}
extern "C" BOOL WINAPI VerQueryValueA(LPCVOID data,LPCSTR block,LPVOID* buffer,PUINT len){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&VerQueryValueA))GetProcAddress(versionModule,"VerQueryValueA"):nullptr;return fn?fn(data,block,buffer,len):FALSE;}
extern "C" DWORD WINAPI VerLanguageNameA(DWORD language,LPSTR buffer,DWORD size){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&VerLanguageNameA))GetProcAddress(versionModule,"VerLanguageNameA"):nullptr;return fn?fn(language,buffer,size):0;}
extern "C" DWORD WINAPI VerFindFileA(DWORD flags,LPSTR file,LPSTR windowsDir,LPSTR appDir,LPSTR currentDir,PUINT currentLen,LPSTR destDir,PUINT destLen){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&VerFindFileA))GetProcAddress(versionModule,"VerFindFileA"):nullptr;return fn?fn(flags,file,windowsDir,appDir,currentDir,currentLen,destDir,destLen):VFF_FILEINUSE;}
extern "C" DWORD WINAPI VerInstallFileA(DWORD flags,LPSTR srcFile,LPSTR destFile,LPSTR srcDir,LPSTR destDir,LPSTR currentDir,LPSTR tempFile,PUINT tempLen){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&VerInstallFileA))GetProcAddress(versionModule,"VerInstallFileA"):nullptr;return fn?fn(flags,srcFile,destFile,srcDir,destDir,currentDir,tempFile,tempLen):VIF_CANNOTREADSRC;}
extern "C" BOOL WINAPI GetFileVersionInfoW(LPCWSTR file,DWORD handle,DWORD len,LPVOID data){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&GetFileVersionInfoW))GetProcAddress(versionModule,"GetFileVersionInfoW"):nullptr;return fn?fn(file,handle,len,data):FALSE;}
extern "C" BOOL WINAPI GetFileVersionInfoExW(DWORD flags,LPCWSTR file,DWORD handle,DWORD len,LPVOID data){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&GetFileVersionInfoExW))GetProcAddress(versionModule,"GetFileVersionInfoExW"):nullptr;return fn?fn(flags,file,handle,len,data):FALSE;}
extern "C" DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR file,LPDWORD handle){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&GetFileVersionInfoSizeW))GetProcAddress(versionModule,"GetFileVersionInfoSizeW"):nullptr;return fn?fn(file,handle):0;}
extern "C" DWORD WINAPI GetFileVersionInfoSizeExW(DWORD flags,LPCWSTR file,LPDWORD handle){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&GetFileVersionInfoSizeExW))GetProcAddress(versionModule,"GetFileVersionInfoSizeExW"):nullptr;return fn?fn(flags,file,handle):0;}
extern "C" BOOL WINAPI VerQueryValueW(LPCVOID data,LPCWSTR block,LPVOID* buffer,PUINT len){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&VerQueryValueW))GetProcAddress(versionModule,"VerQueryValueW"):nullptr;return fn?fn(data,block,buffer,len):FALSE;}
extern "C" DWORD WINAPI VerLanguageNameW(DWORD language,LPWSTR buffer,DWORD size){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&VerLanguageNameW))GetProcAddress(versionModule,"VerLanguageNameW"):nullptr;return fn?fn(language,buffer,size):0;}
extern "C" DWORD WINAPI VerFindFileW(DWORD flags,LPWSTR file,LPWSTR windowsDir,LPWSTR appDir,LPWSTR currentDir,PUINT currentLen,LPWSTR destDir,PUINT destLen){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&VerFindFileW))GetProcAddress(versionModule,"VerFindFileW"):nullptr;return fn?fn(flags,file,windowsDir,appDir,currentDir,currentLen,destDir,destLen):VFF_FILEINUSE;}
extern "C" DWORD WINAPI VerInstallFileW(DWORD flags,LPWSTR srcFile,LPWSTR destFile,LPWSTR srcDir,LPWSTR destDir,LPWSTR currentDir,LPWSTR tempFile,PUINT tempLen){InitOnceExecuteOnce(&versionOnce,initVersion,nullptr,nullptr);auto fn=versionModule?(decltype(&VerInstallFileW))GetProcAddress(versionModule,"VerInstallFileW"):nullptr;return fn?fn(flags,srcFile,destFile,srcDir,destDir,currentDir,tempFile,tempLen):VIF_CANNOTREADSRC;}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){
  wchar_t exePath[MAX_PATH]={};
  DWORD length=GetModuleFileNameW(nullptr,exePath,MAX_PATH);
  if(length&&length<MAX_PATH)
   grassAlignedCapacity::install((unsigned char*)GetModuleHandleW(nullptr),exePath);
  installIAT();
 }
 return TRUE;
}
