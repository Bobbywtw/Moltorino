param([ValidateSet('Snapshot', 'Overlay', 'Stage')][string] $Mode)
$ErrorActionPreference = 'Stop'
$root = $env:SCCACHE_DIR
$overlay = Join-Path $env:GITHUB_WORKSPACE '.sccache-overlay'
$index = Join-Path $env:RUNNER_TEMP 'sccache-base-index.json'
if ($Mode -eq 'Snapshot') {
    $names = @(Get-ChildItem $root -File -Recurse | ForEach-Object { [IO.Path]::GetRelativePath($root, $_.FullName) })
    ConvertTo-Json -InputObject $names | Set-Content $index
    Write-Output "Compiler base entries: $($names.Count)"
} elseif ($Mode -eq 'Overlay') {
    if (Test-Path $overlay) {
        Get-ChildItem $overlay -File -Recurse | ForEach-Object {
            $target = Join-Path $root ([IO.Path]::GetRelativePath($overlay, $_.FullName))
            New-Item -ItemType Directory (Split-Path $target) -Force | Out-Null
            Copy-Item $_.FullName $target -Force
        }
    }
} else {
    $base = [Collections.Generic.HashSet[string]]::new([string[]](Get-Content $index -Raw | ConvertFrom-Json))
    if (Test-Path $overlay) { Remove-Item $overlay -Recurse -Force }
    New-Item -ItemType Directory $overlay -Force | Out-Null
    $count = 0
    $bytes = 0L
    Get-ChildItem $root -File -Recurse | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($root, $_.FullName)
        if (!$base.Contains($relative)) {
            $target = Join-Path $overlay $relative
            New-Item -ItemType Directory (Split-Path $target) -Force | Out-Null
            Copy-Item $_.FullName $target -Force
            $count++
            $bytes += $_.Length
        }
    }
    Write-Output "Compiler overlay entries=$count, bytes=$bytes"
    "entries=$count" >> $env:GITHUB_OUTPUT
    "bytes=$bytes" >> $env:GITHUB_OUTPUT
}
