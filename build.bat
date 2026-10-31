@echo off
rem Builds build\dinput8.dll (in-memory patch) and build\KL_StutterFix.exe (installer,
rem with the DLL embedded). 32-bit, static CRT, Visual Studio Build Tools.
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat
if not exist "%VCVARS%" (
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set VCVARS=%%i\VC\Auxiliary\Build\vcvarsall.bat
)
call "%VCVARS%" x86 >nul 2>nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build

rc /nologo /fo build\dinput8_proxy.res dinput8_proxy.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W4 /LD /Fo:build\ /Fe:build\dinput8.dll dinput8_proxy.c build\dinput8_proxy.res ^
   /link /DEF:dinput8.def || exit /b 1

rc /nologo /fo build\kl_stutter_fix.res kl_stutter_fix.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W4 /Fo:build\ /Fe:build\KL_StutterFix.exe kl_stutter_fix.c build\kl_stutter_fix.res advapi32.lib ^
   /link /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'" || exit /b 1
echo Built build\KL_StutterFix.exe
