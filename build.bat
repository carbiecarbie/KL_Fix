@echo off
rem Builds build\dinput8.dll (in-memory patch), build\xlive.dll (offline GFWL
rem replacement, from xlive\) and build\KL_Fix.exe (installer window, from
rem installer\, with both DLLs embedded).
rem DLLs: 32-bit, static CRT, Visual Studio Build Tools. Installer: C# / WPF on the
rem .NET Framework 4 that ships with Windows.
rem "build.bat test" also runs the xlive.dll API tests.
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

rc /nologo /fo build\xlive.res xlive\xliveless.rc || exit /b 1
cl /nologo /std:c++17 /utf-8 /O2 /MT /W3 /DUNICODE /D_UNICODE /LD /Fo:build\ /Fe:build\xlive.dll ^
   xlive\entry.cpp xlive\xliveless.cpp build\xlive.res user32.lib ws2_32.lib ^
   /link /DEF:xlive\xlive.def /MACHINE:X86 /INCREMENTAL:NO /Brepro || exit /b 1

set NETFX=%WINDIR%\Microsoft.NET\Framework\v4.0.30319
"%NETFX%\csc.exe" /nologo /target:winexe /optimize+ /out:build\KL_Fix.exe ^
   /r:"%NETFX%\WPF\PresentationFramework.dll" /r:"%NETFX%\WPF\PresentationCore.dll" ^
   /r:"%NETFX%\WPF\WindowsBase.dll" /r:"%NETFX%\System.Xaml.dll" ^
   /resource:build\dinput8.dll,dinput8.dll /resource:build\xlive.dll,xlive.dll ^
   installer\*.cs || exit /b 1
echo Built build\KL_Fix.exe
if /i not "%~1"=="test" exit /b 0

cl /nologo /std:c++17 /utf-8 /O2 /MT /W4 /DUNICODE /D_UNICODE /Fo:build\ /Fe:build\xlive_tests.exe ^
   xlive\tests\api_tests.cpp /link /MACHINE:X86 /INCREMENTAL:NO || exit /b 1
set "KL_TEST_PROFILES=%TEMP%\KL_xlive_tests_%RANDOM%%RANDOM%"
build\xlive_tests.exe write build\xlive.dll "%KL_TEST_PROFILES%" || exit /b 1
build\xlive_tests.exe read build\xlive.dll "%KL_TEST_PROFILES%" || exit /b 1
rmdir /s /q "%KL_TEST_PROFILES%"
echo xlive.dll API tests passed
