@echo off
rem D:\github\ESP32\fw\Motor_Control_V1\scripts\run.bat
rem Forwarder batch so CMD callers can run the PowerShell script without typing 'powershell' manually.

setlocal
pushd %~dp0

rem Forward all args to run.ps1
powershell -ExecutionPolicy RemoteSigned -File "%~dp0run.ps1" %*
set rc=%ERRORLEVEL%

popd
exit /b %rc%

@echo off
rem D:\github\ESP32\fw\Motor_Control_V1\scripts\build_and_upload.bat
rem Windows CMD batch script to run PlatformIO tasks in the project root.

echo Running PlatformIO tasks in %CD% ...

:: Run clean
pio run -t clean
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] pio run -t clean failed (exit %ERRORLEVEL%)
    exit /b %ERRORLEVEL%
)

:: Build
pio run
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] pio run (build) failed (exit %ERRORLEVEL%)
    exit /b %ERRORLEVEL%
)

:: Flash firmware
pio run --target upload
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] pio run --target upload failed (exit %ERRORLEVEL%)
    exit /b %ERRORLEVEL%
)

:: Upload filesystem image
pio run --target uploadfs
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] pio run --target uploadfs failed (exit %ERRORLEVEL%)
    exit /b %ERRORLEVEL%
)

echo All PlatformIO tasks completed successfully.
pause
