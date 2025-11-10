<#
D:\github\ESP32\fw\Motor_Control_V1\scripts\build_and_upload.ps1

PowerShell script to run PlatformIO tasks from the project root.
Usage:
  .\build_and_upload.ps1    # from scripts folder
  ..\scripts\build_and_upload.ps1  # from project root
  build_and_upload.ps1 -NoPause  # suppress final pause
#>
param(
    [switch] $NoPause
)

Write-Host "Running PlatformIO tasks in $(Get-Location) ..."

function Run-Check($command) {
    Write-Host "-> $command"
    & cmd /c $command
    $ec = $LASTEXITCODE
    if ($ec -ne 0) {
        Write-Error "Command failed: $command (exit $ec)"
        exit $ec
    }
}

Run-Check "pio run --target upload"
Run-Check "pio run --target uploadfs"

Write-Host "All PlatformIO tasks completed successfully."
if (-not $NoPause) { Read-Host -Prompt "Press Enter to close" }
