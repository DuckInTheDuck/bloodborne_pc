param(
    [int]$TargetPid = 0,
    [int]$DurationSeconds = 600,
    [int]$IntervalSeconds = 5,
    [int]$WaitSeconds = 0,
    [string]$OutputPath = "resource-trend.csv"
)
$ErrorActionPreference = 'Stop'
if ($DurationSeconds -le 0 -or $IntervalSeconds -lt 1) { throw 'Invalid sampling duration or interval' }
if (-not $TargetPid) {
    $games = @(Get-Process -Name bb-probe -ErrorAction SilentlyContinue)
    $deadline = (Get-Date).AddSeconds($WaitSeconds)
    while ($games.Count -eq 0 -and (Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        $games = @(Get-Process -Name bb-probe -ErrorAction SilentlyContinue)
    }
    if ($games.Count -ne 1) { throw 'Start one game instance, or specify -TargetPid' }
    $TargetPid = $games[0].Id
}
$started = [Diagnostics.Stopwatch]::StartNew()
$gpuTool = Get-Command nvidia-smi -ErrorAction SilentlyContinue
Write-Host "Monitoring PID $TargetPid; CSV: $OutputPath (Ctrl+C stops sampling)"
$first = $true
while ($started.Elapsed.TotalSeconds -lt $DurationSeconds) {
    $game = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
    if (-not $game) { break }
    $gpu = @()
    if ($gpuTool) {
        # Device-wide values include other applications; they are not process allocations.
        $raw = & $gpuTool.Source --query-gpu=memory.used,utilization.gpu,temperature.gpu,clocks.current.graphics --format=csv,noheader,nounits 2>$null
        if ($LASTEXITCODE -eq 0 -and $raw) { $gpu = ((@($raw)[0]) -split ',').Trim() }
    }
    $row = [pscustomobject]@{
        Utc = (Get-Date).ToUniversalTime().ToString('o')
        ElapsedSeconds = [math]::Round($started.Elapsed.TotalSeconds, 2)
        Pid = $TargetPid
        PrivateMiB = [math]::Round($game.PrivateMemorySize64 / 1MB, 2)
        WorkingMiB = [math]::Round($game.WorkingSet64 / 1MB, 2)
        Handles = $game.HandleCount
        Threads = $game.Threads.Count
        CpuSeconds = $game.CPU
        DeviceMemoryMiB = if ($gpu.Count -eq 4) { $gpu[0] } else { '' }
        DeviceUtilizationPercent = if ($gpu.Count -eq 4) { $gpu[1] } else { '' }
        DeviceTemperatureC = if ($gpu.Count -eq 4) { $gpu[2] } else { '' }
        DeviceClockMHz = if ($gpu.Count -eq 4) { $gpu[3] } else { '' }
    }
    if ($first) { $row | Export-Csv -LiteralPath $OutputPath -NoTypeInformation; $first = $false }
    else { $row | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Append }
    Start-Sleep -Seconds $IntervalSeconds
}
