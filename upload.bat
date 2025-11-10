@echo off
rem D:\github\ESP32\fw\Motor_Control_V1\scripts\run.bat
rem Forwarder batch so CMD callers can run the PowerShell script without typing 'powershell' manually.

setlocal
pushd %~dp0

rem Forward all args to build_and_upload.ps1
powershell -ExecutionPolicy RemoteSigned -File "%~dp0upload.ps1" %*
set rc=%ERRORLEVEL%

popd
exit /b %rc%
