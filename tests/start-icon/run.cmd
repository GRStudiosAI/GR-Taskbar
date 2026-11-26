@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 exit /b 1
set "GR_TEST_BUILD=%TEMP%\GRTaskbar-StartIcon-Tests-%RANDOM%"
mkdir "%GR_TEST_BUILD%"
if errorlevel 1 exit /b 1
pushd "%GR_TEST_BUILD%"
cl /nologo /W3 "%~dp0painting.c" /Fe:painting.exe /link user32.lib gdi32.lib advapi32.lib
if errorlevel 1 (popd & exit /b 1)
painting.exe
set "GR_TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %GR_TEST_RESULT%
