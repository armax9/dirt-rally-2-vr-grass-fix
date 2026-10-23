@echo off
setlocal
if /I not "%VSCMD_ARG_TGT_ARCH%"=="x64" (echo Run this from an x64 Visual Studio Developer Command Prompt. & exit /b 1)
cl /nologo /std:c11 /O2 /MT /c checksum.c /Fochecksum.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /fp:strict /EHsc /LD /D_VERSION_ /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS version.cpp checksum.obj /link /DEF:version.def /OUT:version.dll
exit /b %errorlevel%
