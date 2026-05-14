# Kills any lingering lab05.exe processes from a previous rehearsal run.
# Useful if a window crashed or a port got stuck.

$procs = Get-Process -Name "lab05" -ErrorAction SilentlyContinue
if ($procs) {
    Write-Host "Killing $($procs.Count) lab05.exe process(es)..." -ForegroundColor Yellow
    $procs | Stop-Process -Force
    Write-Host "Done." -ForegroundColor Green
} else {
    Write-Host "No lab05.exe processes running." -ForegroundColor Green
}
