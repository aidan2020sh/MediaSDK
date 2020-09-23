@echo off
setlocal

if "%1"=="" goto HELP
if "%2"=="" goto HELP
if "%3"=="" goto HELP

set CODEC=%1
set KERNEL=%2
set OPTIONS=%~4

set JIT_TARGET=
if "%3"=="hsw" set JIT_TARGET=gen7_5 & set PLATFORM_NAME=hsw
if "%3"=="bdw" set JIT_TARGET=gen8 & set PLATFORM_NAME=bdw
if "%3"=="skl" set JIT_TARGET=gen9 & set PLATFORM_NAME=skl
if "%3"=="cnl" set JIT_TARGET=gen10 & set PLATFORM_NAME=cnl
if "%3"=="icl" set JIT_TARGET=gen11 & set PLATFORM_NAME=icl
if "%3"=="icllp" set JIT_TARGET=gen11lp & set PLATFORM_NAME=icllp
if "%3"=="tgl" set JIT_TARGET=gen12 & set PLATFORM_NAME=tgl
if "%3"=="tgllp" set JIT_TARGET=gen12lp & set PLATFORM_NAME=tgllp
if "%JIT_TARGET%"=="" goto HELP

set CURDIR=%cd%
set ISA_FILENAME_BUILD=genx_%CODEC%_%KERNEL%
set ISA_FILENAME_FINAL=genx_%CODEC%_%KERNEL%_%PLATFORM_NAME%

echo.
echo ================= Build '%KERNEL%' for %PLATFORM_NAME% =================
del /Q %CURDIR%\src\%ISA_FILENAME_FINAL%_isa.cpp
del /Q %CURDIR%\include\%ISA_FILENAME_FINAL%_isa.h

echo === Setup workspace ===
set TMP=%CURDIR%\..\..\..\..\..\build\win_x64\genx\h265_encode

xcopy /Q /R /Y /I .\include\* %TMP%\include
xcopy /Q /R /Y /I .\src\* %TMP%\src

cd %TMP%

SET VSTOOLS=%VS110COMNTOOLS:~0, -14%vc\bin
IF not EXIST "%VSTOOLS%" GOTO :VSTOOLS_NOT_FOUND
call "%VSTOOLS%\x86_amd64\vcvarsx86_amd64.bat"

SET ICLDIR=%MEDIASDK_ROOT%\cmrt\compiler
IF not EXIST "%ICLDIR%" GOTO :CM_COMPILER_NOT_FOUND
SET PATH=%ICLDIR%\bin;%PATH%
SET INCLUDE=%ICLDIR%\include;%ICLDIR%\include\cm;%MSVSINCL%;%INCLUDE%

echo === Run CM compiler ===
del /Q %ISA_FILENAME_FINAL%.isa %ISA_FILENAME_BUILD%.isa
set CM_COMPILER_OPTIONS=-c -Qxcm_jit_target=%JIT_TARGET% .\src\genx_%CODEC%_%KERNEL%.cpp -Qxcm_print_asm_count -mCM_printregusage /Dtarget_%JIT_TARGET% %OPTIONS%
echo CM compiler options: %CM_COMPILER_OPTIONS%
cmc %CM_COMPILER_OPTIONS%
if not EXIST %ISA_FILENAME_BUILD%.isa goto :BUILD_KERNEL_FAILED
ren %ISA_FILENAME_BUILD%.isa %ISA_FILENAME_FINAL%.isa

echo === Build ISA embedder ===
cl .\src\embed_isa.c /nologo
if not ERRORLEVEL 0 goto :BUILD_EMBEDDER_FAILED

echo === Run ISA embedder ===
embed_isa.exe %ISA_FILENAME_FINAL%.isa
if not ERRORLEVEL 0 goto :EMBED_ISA_FAILED

echo === Copy embedded ISA ===

copy /Y %ISA_FILENAME_FINAL%.cpp %CURDIR%\src\%ISA_FILENAME_FINAL%_isa.cpp
if not ERRORLEVEL 0 goto :COPY_ISA_FAILED
copy /Y %ISA_FILENAME_FINAL%.h %CURDIR%\include\%ISA_FILENAME_FINAL%_isa.h
if not ERRORLEVEL 0 goto :COPY_ISA_FAILED

cd %CURDIR%

echo ================= OK =================
goto :EOF

:VSTOOLS_NOT_FOUND
echo ERROR: MSVS2012 not found
goto :EOF

:CM_COMPILER_NOT_FOUND
echo ERROR: CM compiler not found
goto :EOF

:BUILD_KERNEL_FAILED
echo.
echo ERROR: Failed to build kernel
goto :EOF

:BUILD_EMBEDDER_FAILED
echo.
echo ERROR: Failed to build embed_isa
goto :EOF

:EMBED_ISA_FAILED
echo.
echo ERROR: Failed embed_isa
goto :EOF

:COPY_ISA_FAILED
echo.
echo ERROR: Failed to copy embedded isa
goto :EOF

:HELP
echo Usage: %0 [codec] [kernel] [platform] [options]
echo kernel = name of kernel, eg. deblock, analyze_gradient, etc
echo platform = hsw/bdw
echo options = additional options for CM compiler, eg. /Qxcm_release to strip binaries
exit /b 1
GOTO :EOF
