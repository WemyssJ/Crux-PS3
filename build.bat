@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set PC_FAILED=0
set PS3_FAILED=0

echo ================================================
echo  Building Crux - PC preview (crux_pc.exe)
echo ================================================
"C:\msys64\usr\bin\bash.exe" -lc "export PATH='/mingw64/bin':$PATH; cd '/d/ClaudeCode/Crux PS3'; make -f Makefile.pc"
if errorlevel 1 (
    set PC_FAILED=1
    echo.
    echo *** PC build FAILED ***
) else (
    echo.
    echo PC build OK -^> crux_pc.exe
    if not exist "Build\PC" mkdir "Build\PC"
    copy /y crux_pc.exe "Build\PC\" >nul
    copy /y *.dll "Build\PC\" >nul
    xcopy /y /e /i /q data "Build\PC\data\" >nul
    echo Staged -^> Build\PC\
)

echo.
echo ================================================
echo  Building Crux - PS3 (crux.ppu.self)
echo ================================================
"C:\msys64\usr\bin\bash.exe" -lc "export PATH='/d/PS3/host-win32/spu/bin:/d/PS3/host-win32/ppu/bin:/d/PS3/host-win32/sn/bin:/d/PS3/host-win32/bin:/d/PS3/host-win32/Cg/bin:/c/Program Files (x86)/SN Systems/PS3/bin':$PATH; export CELL_SDK=/d/PS3; export SCE_PS3_ROOT=/d/PS3; cd '/d/ClaudeCode/Crux PS3'; make"
if errorlevel 1 (
    set PS3_FAILED=1
    echo.
    echo *** PS3 build FAILED ***
) else (
    echo.
    echo PS3 build OK -^> crux.ppu.self
    if not exist "Build\PS3" mkdir "Build\PS3"
    copy /y crux.ppu.self "Build\PS3\" >nul
    copy /y crux.ppu.elf "Build\PS3\" >nul
    copy /y vs_quad.vpo "Build\PS3\" >nul
    copy /y fs_quad.fpo "Build\PS3\" >nul
    xcopy /y /e /i /q data "Build\PS3\data\" >nul
    copy /y PS3_DEPLOY_README.txt "Build\PS3\README.txt" >nul
    echo Staged -^> Build\PS3\
)

echo.
echo ================================================
echo  Summary
echo ================================================
if !PC_FAILED!==0 (echo   PC preview : OK  ^(crux_pc.exe^)) else (echo   PC preview : FAILED)
if !PS3_FAILED!==0 (echo   PS3 build  : OK  ^(crux.ppu.self^)) else (echo   PS3 build  : FAILED)
echo ================================================

pause
