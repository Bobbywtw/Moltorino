param([string] $BuildDirectory = 'build/glass-lab', [string] $ReviewDirectory = 'build/lab-review')
$ErrorActionPreference = 'Stop'
# qmlformat parses every source and reparses its output. No -i: never edit QML.
foreach ($qml in Get-ChildItem tools/glass-lab/qml -Filter *.qml) {
    & qmlformat $qml.FullName > $null
    if ($LASTEXITCODE -ne 0) { throw "QML syntax failed: $($qml.Name)" }
}
New-Item -ItemType Directory $ReviewDirectory -Force | Out-Null
$env:QSG_RHI_BACKEND = 'd3d11'
& "$BuildDirectory/MoltorinoGlassChecks.exe" $ReviewDirectory 2>&1 | Tee-Object "$ReviewDirectory/gpu-checks.log"
if ($LASTEXITCODE -ne 0) { throw 'Real GPU QSB/component/composer validation failed' }
foreach ($scene in @('GlassLab', 'GlassIntegrationPreview')) {
    foreach ($state in @('baseline', 'glass', 'resized', 'restored')) {
        $capture = Join-Path $ReviewDirectory "$scene-$state.png"
        if (!(Test-Path $capture) -or (Get-Item $capture).Length -lt 5000) { throw "Missing visual regression capture: $capture" }
    }
}
Get-Content "$ReviewDirectory/gpu-checks.json"
