@echo off
setlocal
set "GR_PAINT_TEST=%TEMP%\GRTaskbar-Paint-Tests-%RANDOM%"
mkdir "%GR_PAINT_TEST%" || exit /b 1
pushd "%GR_PAINT_TEST%"
cl /nologo /W3 "%~dp0painting.c" /Fe:painting.exe
if errorlevel 1 (popd & exit /b 1)
painting.exe
set "GR_RESULT=%ERRORLEVEL%"
popd
exit /b %GR_RESULT%
