@echo off
rem Builds build\uoch.exe (32-bit, static CRT) with the Visual Studio C++ tools.
rem Needs Visual Studio 2017 or later with the "Desktop development with C++" workload.
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo error: vswhere.exe not found. Install Visual Studio with the C++ workload.
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
  echo error: no Visual Studio installation with the C++ tools was found.
  exit /b 1
)

rem vcvars32.bat looks for vswhere.exe on PATH
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1

pushd "%~dp0"
if not exist build mkdir build

cl /nologo /W3 /O2 /MT /DWIN32 /DNDEBUG /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS /Fobuild\ /c Main.c CrackTools.c Crc.c || goto fail
rc /nologo /fo build\uoch.res uoch.rc || goto fail
link /nologo /SUBSYSTEM:WINDOWS /MACHINE:X86 /OUT:build\uoch.exe build\Main.obj build\CrackTools.obj build\Crc.obj build\uoch.res kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib || goto fail

popd
echo.
echo Built build\uoch.exe
exit /b 0

:fail
popd
echo.
echo Build failed.
exit /b 1
