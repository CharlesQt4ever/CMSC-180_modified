# Lab04 Benchmark Script — Different PCs (Table 3)
#
# SETUP REQUIRED:
# 1. Copy lab04.exe and config_multipc.txt to each slave PC
# 2. Edit config_multipc.txt with actual IP addresses
# 3. SSH into each slave PC and start the slave process manually
# 4. Then run this script on the master PC
#
# Example config_multipc.txt:
#   master 192.168.1.100 5000
#   slave  192.168.1.101 5001
#   slave  192.168.1.102 5001
#   slave  192.168.1.103 5001
#   slave  192.168.1.104 5001
#
# On each slave PC (via SSH), run:
#   ./lab04.exe <n> 5001 1 config_multipc.txt
#
# Then on master PC, run this script.

$exe = ".\lab04.exe"
$configFile = "config_multipc.txt"
$nValues = @(4000, 8000, 16000)
$tValues = @(2, 4, 8, 16)
$runs = 3
$masterPort = 5000
$outputFile = "lab04_results_multipc.csv"

# Check config file exists
if (-not (Test-Path $configFile)) {
    Write-Host "ERROR: $configFile not found."
    Write-Host ""
    Write-Host "Create it with actual slave PC IPs:"
    Write-Host "  master <your_ip> $masterPort"
    Write-Host "  slave  <slave1_ip> 5001"
    Write-Host "  slave  <slave2_ip> 5001"
    Write-Host "  ..."
    exit 1
}

Write-Host "=========================================="
Write-Host "Table 3: Different PCs (Multi-PC Mode)"
Write-Host "=========================================="
Write-Host ""
Write-Host "IMPORTANT: Start slave processes on remote PCs BEFORE each run."
Write-Host "This script only runs the master. You must coordinate slaves."
Write-Host ""

# Write CSV header
"n,t,Run,Time_seconds" | Out-File -FilePath $outputFile -Encoding UTF8

$totalTests = $nValues.Count * $tValues.Count * $runs
$currentTest = 0

foreach ($n in $nValues) {
    foreach ($t in $tValues) {
        for ($run = 1; $run -le $runs; $run++) {
            $currentTest++
            Write-Host ""
            Write-Host "--- [$currentTest/$totalTests] n=$n, t=$t, run $run ---"
            Write-Host "Start $t slaves on remote PCs, then press Enter..."
            Read-Host

            $output = & $exe $n $masterPort 0 $configFile 2>&1

            # Extract time
            $timeStr = ""
            foreach ($line in $output) {
                if ($line -match "time elapsed:\s+([\d.]+)\s+seconds") {
                    $timeStr = $Matches[1]
                }
                Write-Host "  $line"
            }

            if ($timeStr -ne "") {
                "$n,$t,$run,$timeStr" | Out-File -FilePath $outputFile -Append -Encoding UTF8
            } else {
                "$n,$t,$run,ERROR" | Out-File -FilePath $outputFile -Append -Encoding UTF8
            }

            Start-Sleep -Milliseconds 500
        }
    }
}

Write-Host ""
Write-Host "=========================================="
Write-Host "Table 3 (Multi-PC) complete!"
Write-Host "Results saved to: $outputFile"
Write-Host "=========================================="
Write-Host ""

# Summary table
Write-Host "TABLE 3: Different PCs, No Core Affinity"
Write-Host "========================================="
Write-Host ("{0,-10} {1,-5} {2,-14} {3,-14} {4,-14} {5,-14}" -f "n", "t", "Run 1", "Run 2", "Run 3", "Average")
Write-Host ("-" * 73)

$results = Import-Csv $outputFile

foreach ($n in $nValues) {
    foreach ($t in $tValues) {
        $filtered = $results | Where-Object { [int]$_.n -eq $n -and [int]$_.t -eq $t }
        $times = $filtered | ForEach-Object { [double]$_.Time_seconds }

        if ($times.Count -eq $runs) {
            $avg = ($times | Measure-Object -Average).Average
            $r1 = "{0:F6}" -f $times[0]
            $r2 = "{0:F6}" -f $times[1]
            $r3 = "{0:F6}" -f $times[2]
            $avgStr = "{0:F6}" -f $avg
            Write-Host ("{0,-10} {1,-5} {2,-14} {3,-14} {4,-14} {5,-14}" -f $n, $t, $r1, $r2, $r3, $avgStr)
        }
    }
    Write-Host ""
}
