# D:\github\ESP32\fw\Motor_Control_V1\scripts\build_and_upload.ps1
# PowerShell script to run PlatformIO tasks from the project root.
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

Run-Check "pio run -t clean"
Run-Check "pio run"
Run-Check "pio run --target upload"
Run-Check "pio run --target uploadfs"

Write-Host "All PlatformIO tasks completed successfully."
if (-not $NoPause) { Read-Host -Prompt "Press Enter to close" }
