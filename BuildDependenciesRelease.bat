@echo off
setlocal EnableExtensions DisableDelayedExpansion
pushd "%~dp0" || exit /b 1
if not defined VSINSTALLDIR (
    echo ERROR: Run from a Visual Studio Developer Command Prompt.
    goto :fail
)
set "GR_VSINSTALLDIR=%VSINSTALLDIR%"
if "%GR_VSINSTALLDIR:~-1%"=="\" set "GR_VSINSTALLDIR=%GR_VSINSTALLDIR:~0,-1%"
if not exist "%GR_VSINSTALLDIR%\Common7\IDE\devenv.exe" if not exist "%GR_VSINSTALLDIR%\MSBuild\Current\Bin\MSBuild.exe" (
    echo ERROR: Invalid Visual Studio installation.
    goto :fail
)
where cmake.exe >nul 2>&1 || goto :fail
if not exist "libs\zlib\CMakeLists.txt" goto :fail
call :build x64 x64 || goto :fail
call :build arm64 ARM64 || goto :fail
popd
exit /b 0

:build
cmake -S libs/zlib -B libs/zlib/build/%~1 -G "Visual Studio 18 2026" -A %~2 -D"CMAKE_GENERATOR_INSTANCE:PATH=%GR_VSINSTALLDIR%" -D"CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
if errorlevel 1 exit /b 1
cmake --build libs/zlib/build/%~1 --config Release
if errorlevel 1 exit /b 1
exit /b 0

:fail
echo ERROR: GR Taskbar dependency build failed. Review the output above.
popd
exit /b 1
