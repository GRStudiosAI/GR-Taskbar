@echo off
setlocal
where cl >nul 2>nul
if errorlevel 1 exit /b 1
set "GR_MIGRATION_TEST_BUILD=%TEMP%\GRTaskbar-Migration-Build-%RANDOM%"
mkdir "%GR_MIGRATION_TEST_BUILD%"
if errorlevel 1 exit /b 1
pushd "%GR_MIGRATION_TEST_BUILD%"
cl /nologo /W3 "%~dp0registry.c" /Fe:migration.exe
if errorlevel 1 (popd & exit /b 1)
migration.exe
set "GR_MIGRATION_TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %GR_MIGRATION_TEST_RESULT%
