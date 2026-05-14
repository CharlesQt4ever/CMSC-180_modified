# Local rehearsal for LRP05 presentation: 1 master + 4 slaves all on this PC.
# Opens 5 separate PowerShell windows so each slave's output is visible.
#
# Usage:  .\run_local_demo.ps1
# Stop:   close the windows (or run .\stop_local_demo.ps1)

$dir = $PSScriptRoot
$exe = Join-Path $dir "lab05.exe"
$cfg = "config_local.txt"

if (-not (Test-Path $exe)) {
    Write-Host "ERROR: lab05.exe not found at $exe" -ForegroundColor Red
    exit 1
}

Write-Host "Launching 4 slaves on ports 5001-5004..." -ForegroundColor Cyan

# Slave 1 -> port 5001, pinned to core 0
Start-Process powershell -ArgumentList "-NoExit", "-Command",
    "`$Host.UI.RawUI.WindowTitle='SLAVE 1 (port 5001)'; cd '$dir'; .\lab05.exe 4 5001 1 $cfg 0"

# Slave 2 -> port 5002, pinned to core 0
Start-Process powershell -ArgumentList "-NoExit", "-Command",
    "`$Host.UI.RawUI.WindowTitle='SLAVE 2 (port 5002)'; cd '$dir'; .\lab05.exe 4 5002 1 $cfg 0"

# Slave 3 -> port 5003, pinned to core 0
Start-Process powershell -ArgumentList "-NoExit", "-Command",
    "`$Host.UI.RawUI.WindowTitle='SLAVE 3 (port 5003)'; cd '$dir'; .\lab05.exe 4 5003 1 $cfg 0"

# Slave 4 -> port 5004, pinned to core 0
Start-Process powershell -ArgumentList "-NoExit", "-Command",
    "`$Host.UI.RawUI.WindowTitle='SLAVE 4 (port 5004)'; cd '$dir'; .\lab05.exe 4 5004 1 $cfg 0"

Write-Host "Waiting 2 seconds for slaves to bind their sockets..." -ForegroundColor Cyan
Start-Sleep -Seconds 2

Write-Host "Launching MASTER on port 5000..." -ForegroundColor Cyan

# Master (s=0). No 5th arg = random matrix mode.
Start-Process powershell -ArgumentList "-NoExit", "-Command",
    "`$Host.UI.RawUI.WindowTitle='MASTER'; cd '$dir'; .\lab05.exe 4 5000 0 $cfg"

Write-Host "All 5 windows launched. Arrange them on screen for the demo." -ForegroundColor Green
