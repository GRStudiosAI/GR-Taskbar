@echo off
setlocal
set "GR_UPDATE_TEST=%TEMP%\GRTaskbar-Update-Tests-%RANDOM%"
mkdir "%GR_UPDATE_TEST%" || exit /b 1
pushd "%GR_UPDATE_TEST%"
cl /nologo /EHsc /W3 /I"%~dp0..\..\packages\Microsoft.Windows.ImplementationLibrary.1.0.260126.7\include" "%~dp0verification.cpp" /Fe:verification.exe
if errorlevel 1 (popd & exit /b 1)
verification.exe "D:\Projects\work\outputs\GR-Taskbar-Setup.exe"
set "GR_RESULT=%ERRORLEVEL%"
popd
exit /b %GR_RESULT%
