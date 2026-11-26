@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 (
  echo Run this from a Visual Studio x64 Native Tools command prompt.
  exit /b 1
)
set "GR_TEST_SOURCE=%~dp0servicing.c"
set "GR_TEST_BUILD=%TEMP%\GRTaskbar-Servicing-Tests-%RANDOM%"
mkdir "%GR_TEST_BUILD%"
if errorlevel 1 exit /b 1
pushd "%GR_TEST_BUILD%"
cl /nologo /W3 /DUNICODE /D_UNICODE "%GR_TEST_SOURCE%" /Fe:servicing-tests.exe /link user32.lib gdi32.lib advapi32.lib shell32.lib shlwapi.lib version.lib
if errorlevel 1 (popd & exit /b 1)
servicing-tests.exe
set "GR_TEST_RESULT=%ERRORLEVEL%"
echo Test files retained at %GR_TEST_BUILD%
popd
exit /b %GR_TEST_RESULT%
