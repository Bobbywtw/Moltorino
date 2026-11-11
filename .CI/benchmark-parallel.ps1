param([string] $BuildDirectory = 'build')
$ErrorActionPreference = 'Stop'
$build = (Resolve-Path $BuildDirectory).Path
$names = @('SplitInput.cpp', 'ChannelView.cpp', 'Split.cpp', 'SplitHeader.cpp', 'TwitchMessageBuilder.cpp', 'WindowManager.cpp', 'EmotePopup.cpp', 'SettingsDialog.cpp')
$commands = Get-Content (Join-Path $build 'compile_commands.json') -Raw | ConvertFrom-Json
$targets = @($commands | Where-Object { (Split-Path $_.file -Leaf) -in $names } | ForEach-Object {
    if ($_.output) { if ([IO.Path]::IsPathRooted($_.output)) { [IO.Path]::GetRelativePath($build, $_.output) } else { $_.output } } elseif ($_.command -match '/Fo([^\s]+)') { $Matches[1].Trim('"') }
})
if ($targets.Count -lt 6) { throw 'Not enough real translation units for a parallelism comparison' }
$results = @()
$oldRecache = $env:SCCACHE_RECACHE
try {
    $env:SCCACHE_RECACHE = '1'
    foreach ($jobs in @(4, 6)) {
        foreach ($target in $targets) { Remove-Item (Join-Path $build $target) -Force }
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $output = Join-Path $build "metrics/parallel-$jobs.log"
        $process = Start-Process ninja -ArgumentList (@('-C', $build, "-j$jobs") + $targets) -PassThru -NoNewWindow -RedirectStandardOutput $output -RedirectStandardError "$output.err"
        $null = $process.Handle
        $minimumFreeMB = [double]::PositiveInfinity
        $peakCompilerMB = 0
        while (!$process.HasExited) {
            $os = Get-CimInstance Win32_OperatingSystem
            $minimumFreeMB = [Math]::Min($minimumFreeMB, $os.FreePhysicalMemory / 1024)
            $working = (Get-Process cl -ErrorAction SilentlyContinue | Measure-Object WorkingSet64 -Sum).Sum / 1MB
            $peakCompilerMB = [Math]::Max($peakCompilerMB, $working)
            Start-Sleep -Milliseconds 500
        }
        $process.WaitForExit()
        $exit = $process.ExitCode
        $result = @{ jobs = $jobs; seconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3); exitCode = $exit; minimumFreeMB = [Math]::Round($minimumFreeMB); peakCompilerMB = [Math]::Round($peakCompilerMB); objects = $targets.Count }
        $results += $result
        Write-Output ($result | ConvertTo-Json -Compress)
        if ($exit -ne 0) { Get-Content "$output.err" -Tail 30; break }
    }
} finally { $env:SCCACHE_RECACHE = $oldRecache }
$results | ConvertTo-Json | Set-Content (Join-Path $build 'metrics/parallelism.json')
# This is an explicit experiment after the deliverable was already built.
# Keep production at four unless the six-job result is faster and has headroom.
if ($results.Count -eq 2 -and $results[1].exitCode -eq 0 -and $results[1].minimumFreeMB -gt 2048 -and $results[1].seconds -lt $results[0].seconds * 0.95) {
    Write-Output 'Six jobs improved this representative sample with at least 2GB free; evaluate before changing the full-build default.'
} else { Write-Output 'Retain the reliable four-job production default.' }
