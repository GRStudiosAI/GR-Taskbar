@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 exit /b 1
set "GR_CURSOR_TEST_BUILD=%TEMP%\GRTaskbar-Cursor-Build-%RANDOM%"
mkdir "%GR_CURSOR_TEST_BUILD%"
if errorlevel 1 exit /b 1
pushd "%GR_CURSOR_TEST_BUILD%"
cl /nologo /W3 "%~dp0model.c" /Fe:cursor-model.exe
if errorlevel 1 (popd & exit /b 1)
cursor-model.exe
set "GR_CURSOR_TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %GR_CURSOR_TEST_RESULT%
