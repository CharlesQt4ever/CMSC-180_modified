# Lab05 Benchmark Script — Single PC, NO Core Affinity (Table 1 + Table 2)
# Launches t slave processes + 1 master. Captures master time AND
# the maximum slave compute time per run.

$exe = ".\lab05.exe"
$nValues = @(4000, 8000, 16000)
$tValues = @(2, 4, 8, 16)
$runs = 3
$basePort = 5000
$outputFile = "lab05_results_no_affinity.csv"
$logDir = "lab05_slavelogs_t1"

if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }

# CSV header — both metrics in one file
"n,t,Run,Master_seconds,Slave_max_seconds" | Out-File -FilePath $outputFile -Encoding UTF8

$totalTests = $nValues.Count * $tValues.Count * $runs
$currentTest = 0

foreach ($n in $nValues) {
    foreach ($t in $tValues) {
        $configFile = "config_t${t}.txt"
        $configLines = @("master 127.0.0.1 $basePort")
        for ($i = 1; $i -le $t; $i++) {
            $slavePort = $basePort + $i
            $configLines += "slave 127.0.0.1 $slavePort"
        }
        $configLines | Out-File -FilePath $configFile -Encoding ASCII

        for ($run = 1; $run -le $runs; $run++) {
            $currentTest++
            Write-Host "[$currentTest/$totalTests] n=$n, t=$t, run $run ..." -NoNewline

            # start slaves with per-slave log files (so we can read time elapsed)
            $slaves = @()
            $slaveLogs = @()
            for ($i = 1; $i -le $t; $i++) {
                $slavePort = $basePort + $i
                $log = Join-Path $logDir ("n{0}_t{1}_r{2}_s{3}.log" -f $n, $t, $run, $i)
                $slaveLogs += $log
                $slave = Start-Process -FilePath $exe `
                    -ArgumentList "$n $slavePort $i $configFile" `
                    -PassThru -WindowStyle Hidden `
                    -RedirectStandardOutput $log -RedirectStandardError "NUL"
                $slaves += $slave
            }

            Start-Sleep -Milliseconds 1500

            $masterPort = $basePort
            $output = & $exe $n $masterPort 0 $configFile 2>&1

            # extract master time
            $masterTime = ""
            foreach ($line in $output) {
                if ($line -match "time elapsed:\s+([\d.]+)\s+seconds") {
                    $masterTime = $Matches[1]
                }
            }

            # wait for slaves to finish writing their logs
            foreach ($s in $slaves) {
                try {
                    if (!$s.HasExited) {
                        $s.WaitForExit(5000)
                        if (!$s.HasExited) { $s.Kill() }
                    }
                } catch { }
            }

            # extract max slave time
            $slaveMax = $null
            foreach ($log in $slaveLogs) {
                if (Test-Path $log) {
                    $logContent = Get-Content $log -Raw
                    $matches = [regex]::Matches($logContent, "time elapsed:\s+([\d.]+)\s+seconds")
                    foreach ($m in $matches) {
                        $val = [double]$m.Groups[1].Value
                        if (($null -eq $slaveMax) -or ($val -gt $slaveMax)) { $slaveMax = $val }
                    }
                }
            }

            if (($masterTime -ne "") -and ($null -ne $slaveMax)) {
                $slaveStr = "{0:F6}" -f $slaveMax
                Write-Host " master=$masterTime, slave_max=$slaveStr"
                "$n,$t,$run,$masterTime,$slaveStr" | Out-File -FilePath $outputFile -Append -Encoding UTF8
            } else {
                Write-Host " ERROR (master=$masterTime, slave_max=$slaveMax)"
                "$n,$t,$run,ERROR,ERROR" | Out-File -FilePath $outputFile -Append -Encoding UTF8
            }

            Start-Sleep -Milliseconds 500
        }

        Remove-Item -Path $configFile -ErrorAction SilentlyContinue
    }
}

Write-Host ""
Write-Host "=========================================="
Write-Host "Lab05 (No Core Affinity) sweep complete!"
Write-Host "Results saved to: $outputFile"
Write-Host "Slave logs: $logDir"
Write-Host "=========================================="
Write-Host ""

# Summary tables
$results = Import-Csv $outputFile

Write-Host "TABLE 1 (master) — Single PC, No Core Affinity"
Write-Host "==============================================="
Write-Host ("{0,-10} {1,-5} {2,-14} {3,-14} {4,-14} {5,-14}" -f "n", "t", "Run 1", "Run 2", "Run 3", "Average")
Write-Host ("-" * 73)
foreach ($n in $nValues) {
    foreach ($t in $tValues) {
        $filtered = $results | Where-Object { [int]$_.n -eq $n -and [int]$_.t -eq $t -and $_.Master_seconds -ne "ERROR" }
        $times = $filtered | ForEach-Object { [double]$_.Master_seconds }
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

Write-Host "TABLE 2 (slave max) — Single PC, No Core Affinity"
Write-Host "================================================="
Write-Host ("{0,-10} {1,-5} {2,-14} {3,-14} {4,-14} {5,-14}" -f "n", "t", "Max R1", "Max R2", "Max R3", "Average")
Write-Host ("-" * 73)
foreach ($n in $nValues) {
    foreach ($t in $tValues) {
        $filtered = $results | Where-Object { [int]$_.n -eq $n -and [int]$_.t -eq $t -and $_.Slave_max_seconds -ne "ERROR" }
        $times = $filtered | ForEach-Object { [double]$_.Slave_max_seconds }
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
