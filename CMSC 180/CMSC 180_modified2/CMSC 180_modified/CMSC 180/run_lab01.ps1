# Lab01 Benchmark Script (Serial)
# Table from Lab01 instructions + Lab03 comparison values

$exe = ".\lab01.exe"
$runs = 3

# Lab01 original table values + Lab03 comparison values (25000, 30000, 40000)
$nValues = @(100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 4000, 8000, 16000, 20000, 25000, 30000, 40000)

$outputFile = "lab01_results.csv"

"n,Run,Time_seconds" | Out-File -FilePath $outputFile -Encoding UTF8

$totalTests = $nValues.Count * $runs
$currentTest = 0

foreach ($n in $nValues) {
    for ($run = 1; $run -le $runs; $run++) {
        $currentTest++
        Write-Host "[$currentTest/$totalTests] Running: n=$n, run $run ..." -NoNewline

        # lab01 reads n from stdin
        $output = echo $n | & $exe 2>&1

        if ($output -match "time elapsed:\s+([\d.]+)\s+seconds") {
            $time = $Matches[1]
            Write-Host " $time seconds"
            "$n,$run,$time" | Out-File -FilePath $outputFile -Append -Encoding UTF8
        } else {
            Write-Host " ERROR: $output"
            "$n,$run,ERROR" | Out-File -FilePath $outputFile -Append -Encoding UTF8
        }
    }
}

Write-Host ""
Write-Host "=========================================="
Write-Host "All Lab01 benchmarks complete!"
Write-Host "Results saved to: $outputFile"
Write-Host "=========================================="
Write-Host ""

# Summary table
Write-Host "SUMMARY TABLE"
Write-Host "============="
Write-Host ("{0,-10} {1,-14} {2,-14} {3,-14} {4,-14}" -f "n", "Run 1", "Run 2", "Run 3", "Average")
Write-Host ("-" * 66)

$results = Import-Csv $outputFile

foreach ($n in $nValues) {
    $filtered = $results | Where-Object { [int]$_.n -eq $n }
    $times = $filtered | ForEach-Object { [double]$_.Time_seconds }

    if ($times.Count -eq $runs) {
        $avg = ($times | Measure-Object -Average).Average
        $r1 = "{0:F6}" -f $times[0]
        $r2 = "{0:F6}" -f $times[1]
        $r3 = "{0:F6}" -f $times[2]
        $avgStr = "{0:F6}" -f $avg
        Write-Host ("{0,-10} {1,-14} {2,-14} {3,-14} {4,-14}" -f $n, $r1, $r2, $r3, $avgStr)
    }
}
