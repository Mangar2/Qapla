# PowerShell script to compare wmtest results
# Compares node counts between baseline and new version

param(
    [string]$BaselineFile = ".\build\Release\baseline.txt",
    [string]$NewFile = ".\build\Release\wmtest12.txt"
)

Write-Host "Comparing wmtest results..." -ForegroundColor Cyan
Write-Host "Baseline: $BaselineFile"
Write-Host "New:      $NewFile"
Write-Host ""

# Check if baseline exists
if (-not (Test-Path $BaselineFile)) {
    Write-Host "ERROR: Baseline file not found!" -ForegroundColor Red
    Write-Host "Please create baseline first:" -ForegroundColor Yellow
    Write-Host "  Copy-Item build\Release\Qapla.exe build\Release\Qapla_baseline.exe" -ForegroundColor Gray
    Write-Host "  echo `"wmtest sd 12``nquit`" | .\build\Release\Qapla_baseline.exe > .\build\Release\baseline.txt" -ForegroundColor Gray
    exit 1
}

# Create new wmtest12.txt by running current Qapla.exe
Write-Host "Running wmtest on current version..." -ForegroundColor Cyan
& { Write-Output "wmtest sd 12`nquit" | .\build\Release\Qapla.exe } > $NewFile 2>&1
Write-Host "Completed. Comparing results..." -ForegroundColor Cyan
Write-Host ""

# Read both files
$baselineLines = Get-Content $BaselineFile
$newLines = Get-Content $NewFile

# Extract position lines (those containing "nodes:")
$baselinePositions = $baselineLines | Where-Object { $_ -match 'nodes:\s+(\d+)' }
$newPositions = $newLines | Where-Object { $_ -match 'nodes:\s+(\d+)' }

if ($baselinePositions.Count -ne $newPositions.Count) {
    Write-Host "ERROR: Different number of positions!" -ForegroundColor Red
    Write-Host "Baseline: $($baselinePositions.Count) positions"
    Write-Host "New:      $($newPositions.Count) positions"
    exit 1
}

# Compare each position
$differences = 0
$firstDifference = $null

for ($i = 0; $i -lt $baselinePositions.Count; $i++) {
    $baselineLine = $baselinePositions[$i]
    $newLine = $newPositions[$i]
    
    # Extract node counts using regex
    if ($baselineLine -match 'nodes:\s+(\d+)') {
        $baselineNodes = [int64]$Matches[1]
    }
    if ($newLine -match 'nodes:\s+(\d+)') {
        $newNodes = [int64]$Matches[1]
    }
    
    # Extract position ID
    if ($baselineLine -match 'id\s+"([^"]+)"') {
        $positionId = $Matches[1]
    }
    
    if ($baselineNodes -ne $newNodes) {
        $differences++
        if ($null -eq $firstDifference) {
            $firstDifference = @{
                Index = $i + 1
                Id = $positionId
                BaselineNodes = $baselineNodes
                NewNodes = $newNodes
                Difference = $newNodes - $baselineNodes
                BaselineLine = $baselineLine
                NewLine = $newLine
            }
        }
    }
}

# Extract total node counts from summary line
$baselineSummary = $baselineLines | Where-Object { $_ -match 'Total nodes searched:\s+(\d+)' } | Select-Object -First 1
$newSummary = $newLines | Where-Object { $_ -match 'Total nodes searched:\s+(\d+)' } | Select-Object -First 1

if ($baselineSummary -match 'Total nodes searched:\s+(\d+)') {
    $baselineTotalNodes = [int64]$Matches[1]
}
if ($newSummary -match 'Total nodes searched:\s+(\d+)') {
    $newTotalNodes = [int64]$Matches[1]
}

# Extract time used from summary line
$baselineTimeLine = $baselineLines | Where-Object { $_ -match 'Time used \(s\):\s+([\d.]+)' } | Select-Object -First 1
$newTimeLine = $newLines | Where-Object { $_ -match 'Time used \(s\):\s+([\d.]+)' } | Select-Object -First 1

if ($baselineTimeLine -match 'Time used \(s\):\s+([\d.]+)') {
    $baselineTime = [double]$Matches[1]
}
if ($newTimeLine -match 'Time used \(s\):\s+([\d.]+)') {
    $newTime = [double]$Matches[1]
}

# Report results
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "COMPARISON RESULTS" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

if ($differences -eq 0) {
    Write-Host "IDENTICAL - All positions have identical node counts!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Total nodes: $baselineTotalNodes" -ForegroundColor Green
    Write-Host ""
    Write-Host "Time baseline: $baselineTime s" -ForegroundColor White
    Write-Host "Time new:      $newTime s" -ForegroundColor White
    
    $timeDiff = $newTime - $baselineTime
    $timePercent = ($timeDiff / $baselineTime) * 100
    $timeColor = if ([Math]::Abs($timePercent) -le 1.0) { "Green" } elseif ($timeDiff -gt 0) { "Yellow" } else { "Green" }
    Write-Host "Difference:    $($timeDiff.ToString("F3")) s ($($timePercent.ToString("F2"))%)" -ForegroundColor $timeColor
    
    if ([Math]::Abs($timePercent) -le 1.0) {
        Write-Host "Time difference is within acceptable range (<= 1%)" -ForegroundColor Green
    }
} else {
    Write-Host "DIFFERENCES FOUND" -ForegroundColor Red
    Write-Host ""
    Write-Host "Positions with different node counts: $differences / $($baselinePositions.Count)" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Total nodes baseline: $baselineTotalNodes" -ForegroundColor White
    Write-Host "Total nodes new:      $newTotalNodes" -ForegroundColor White
    
    $nodeDiff = $newTotalNodes - $baselineTotalNodes
    $nodePercent = ($nodeDiff / [double]$baselineTotalNodes) * 100
    $diffColor = if ($nodeDiff -gt 0) { "Red" } else { "Green" }
    Write-Host "Difference:           $nodeDiff nodes ($($nodePercent.ToString("F2"))%)" -ForegroundColor $diffColor
    Write-Host ""
    Write-Host "Time baseline: $baselineTime s" -ForegroundColor White
    Write-Host "Time new:      $newTime s" -ForegroundColor White
    
    $timeDiff = $newTime - $baselineTime
    $timePercent = ($timeDiff / $baselineTime) * 100
    $timeColor = if ($timeDiff -gt 0) { "Yellow" } else { "Green" }
    Write-Host "Difference:    $($timeDiff.ToString("F3")) s ($($timePercent.ToString("F2"))%)" -ForegroundColor $timeColor
    Write-Host ""
    Write-Host "----------------------------------------" -ForegroundColor Yellow
    Write-Host "FIRST DIFFERENCE:" -ForegroundColor Yellow
    Write-Host "----------------------------------------" -ForegroundColor Yellow
    Write-Host "Position #$($firstDifference.Index): $($firstDifference.Id)" -ForegroundColor White
    Write-Host "Baseline nodes: $($firstDifference.BaselineNodes)" -ForegroundColor White
    Write-Host "New nodes:      $($firstDifference.NewNodes)" -ForegroundColor White
    
    $posDiff = $firstDifference.Difference
    $posDiffColor = if ($posDiff -gt 0) { "Red" } else { "Green" }
    Write-Host "Difference:     $posDiff nodes" -ForegroundColor $posDiffColor
    Write-Host ""
    Write-Host "Baseline: $($firstDifference.BaselineLine)" -ForegroundColor Gray
    Write-Host "New:      $($firstDifference.NewLine)" -ForegroundColor Gray
    
    exit 1
}

Write-Host "============================================" -ForegroundColor Cyan
