# Lab04 Benchmark Script — Single PC, NO Core Affinity (Table 1)
# Launches t slave processes + 1 master, collects master timing.

$exe = ".\lab04.exe"
$nValues = @(4000, 8000, 16000)
$tValues = @(2, 4, 8, 16)
$runs = 3
$basePort = 5000
$outputFile = "lab04_results_no_affinity.csv"

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

            # Start t slave processes
            $slaves = @()
            for ($i = 1; $i -le $t; $i++) {
                $slavePort = $basePort + $i
                $slave = Start-Process -FilePath $exe `
                    -ArgumentList "$n $slavePort 1 $configFile" `
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
                Write-Host " $timeStr seconds"
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
Write-Host "Table 1 (No Core Affinity) complete!"
Write-Host "Results saved to: $outputFile"
Write-Host "=========================================="
Write-Host ""

# Summary table
Write-Host "TABLE 1: Single PC, No Core Affinity"
Write-Host "====================================="
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
