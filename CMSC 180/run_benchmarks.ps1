# Lab03 Benchmark Script
# Runs all test configurations and outputs results to CSV

$exe = ".\lab03.exe"
$nValues = @(25000, 30000, 40000)
$tValues = @(1, 2, 4, 8, 16, 32, 64)
$runs = 3

$outputFile = "lab03_results.csv"

# Write CSV header
"n,t,Run,Time_seconds" | Out-File -FilePath $outputFile -Encoding UTF8

$totalTests = $nValues.Count * $tValues.Count * $runs
$currentTest = 0

foreach ($n in $nValues) {
    foreach ($t in $tValues) {
        for ($run = 1; $run -le $runs; $run++) {
            $currentTest++
            Write-Host "[$currentTest/$totalTests] Running: n=$n, t=$t, run $run ..." -NoNewline

            $output = & $exe $n $t 2>&1
            
            # Extract the time from "time elapsed: X.XXXXXX seconds"
            if ($output -match "time elapsed:\s+([\d.]+)\s+seconds") {
                $time = $Matches[1]
                Write-Host " $time seconds"
                "$n,$t,$run,$time" | Out-File -FilePath $outputFile -Append -Encoding UTF8
            } else {
                Write-Host " ERROR: $output"
                "$n,$t,$run,ERROR" | Out-File -FilePath $outputFile -Append -Encoding UTF8
            }
        }
    }
}

Write-Host ""
Write-Host "=========================================="
Write-Host "All benchmarks complete! Results saved to: $outputFile"
Write-Host "=========================================="
Write-Host ""

# Display summary table with averages
Write-Host "SUMMARY TABLE (Averages)"
Write-Host "========================"
Write-Host ("{0,-10} {1,-5} {2,-12} {3,-12} {4,-12} {5,-12}" -f "n", "t", "Run 1", "Run 2", "Run 3", "Average")
Write-Host ("-" * 65)

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
            Write-Host ("{0,-10} {1,-5} {2,-12} {3,-12} {4,-12} {5,-12}" -f $n, $t, $r1, $r2, $r3, $avgStr)
        }
    }
    Write-Host ""
}
