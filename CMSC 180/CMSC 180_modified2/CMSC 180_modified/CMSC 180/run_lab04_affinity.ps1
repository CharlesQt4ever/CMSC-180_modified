# Lab04 Benchmark Script — Single PC, WITH Core Affinity (Table 2)
# Each slave is pinned to a specific core. Core 0 is reserved for OS.

$exe = ".\lab04.exe"
$nValues = @(4000, 8000, 16000)
$tValues = @(2, 4, 8, 16)
$runs = 3
$basePort = 5000
$outputFile = "lab04_results_affinity.csv"

# Detect available cores (reserve core 0 for OS)
$numCores = (Get-WmiObject Win32_Processor).NumberOfLogicalProcessors
if (-not $numCores) { $numCores = 8 }
$availableCores = $numCores - 1   # cores 1 through numCores-1
Write-Host "System has $numCores logical cores. Using cores 1-$($numCores-1) for slaves."
Write-Host ""

# Write CSV header
"n,t,Run,Time_seconds" | Out-File -FilePath $outputFile -Encoding UTF8

$totalTests = $nValues.Count * $tValues.Count * $runs
$currentTest = 0

foreach ($n in $nValues) {
    foreach ($t in $tValues) {
        # Generate config file for this t value
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

            # Start t slave processes with core affinity
            $slaves = @()
            for ($i = 1; $i -le $t; $i++) {
                $slavePort = $basePort + $i
                # Assign cores round-robin: 1, 2, ..., availableCores, 1, 2, ...
                $coreId = (($i - 1) % $availableCores) + 1
                $slave = Start-Process -FilePath $exe `
                    -ArgumentList "$n $slavePort 1 $configFile $coreId" `
                    -PassThru -WindowStyle Hidden `
                    -RedirectStandardOutput "NUL" -RedirectStandardError "NUL"
                $slaves += $slave
            }

            # Wait for slaves to be ready
            Start-Sleep -Milliseconds 1500

            # Run master and capture output
            $masterPort = $basePort
            $output = & $exe $n $masterPort 0 $configFile 2>&1

            # Extract time
            $timeStr = ""
            foreach ($line in $output) {
                if ($line -match "time elapsed:\s+([\d.]+)\s+seconds") {
                    $timeStr = $Matches[1]
                }
            }

            if ($timeStr -ne "") {
                Write-Host " $timeStr seconds (cores pinned)"
                "$n,$t,$run,$timeStr" | Out-File -FilePath $outputFile -Append -Encoding UTF8
            } else {
                Write-Host " ERROR"
                "$n,$t,$run,ERROR" | Out-File -FilePath $outputFile -Append -Encoding UTF8
            }

            # Clean up slaves
            foreach ($s in $slaves) {
                try {
                    if (!$s.HasExited) {
                        $s.WaitForExit(3000)
                        if (!$s.HasExited) { $s.Kill() }
                    }
                } catch { }
            }

            # Brief pause between runs to release ports
            Start-Sleep -Milliseconds 500
        }

        # Clean up config file
        Remove-Item -Path $configFile -ErrorAction SilentlyContinue
    }
}

Write-Host ""
Write-Host "=========================================="
Write-Host "Table 2 (Core Affinity) complete!"
Write-Host "Results saved to: $outputFile"
Write-Host "=========================================="
Write-Host ""

# Summary table
Write-Host "TABLE 2: Single PC, With Core Affinity"
Write-Host "======================================="
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
