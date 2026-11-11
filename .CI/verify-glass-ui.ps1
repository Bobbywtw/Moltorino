param(
    [Parameter(Mandatory = $true)][string] $PackageDirectory,
    [Parameter(Mandatory = $true)][string] $ReviewDirectory
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PackageDirectory 'Moltorino7.exe'
foreach ($marker in @('modes', 'portable')) {
    if (Test-Path (Join-Path $PackageDirectory $marker)) {
        throw "Package must use the existing normal profile, but contains $marker"
    }
}
function Invoke-Package([string[]] $Arguments) {
    $process = Start-Process -FilePath $exe -ArgumentList $Arguments -PassThru -NoNewWindow
    if (!$process.WaitForExit(60000)) {
        $process.Kill()
        throw "Moltorino launch timed out: $Arguments"
    }
    if ($process.ExitCode -ne 0) {
        if ($env:CHATTERINO_LOG_TO_FILE -and (Test-Path $env:CHATTERINO_LOG_TO_FILE)) {
            Get-Content $env:CHATTERINO_LOG_TO_FILE -Tail 100
        }
        if ($process.ExitCode -eq 11) {
            $debugger = Join-Path $env:GITHUB_WORKSPACE 'build/diagnostics/debug-windows-startup.exe'
            & $debugger $exe (Join-Path $env:GITHUB_WORKSPACE 'build/bin')
            if ($env:CHATTERINO_LOG_TO_FILE -and (Test-Path $env:CHATTERINO_LOG_TO_FILE)) {
                Get-Content $env:CHATTERINO_LOG_TO_FILE -Tail 100
            }
        }
        throw "Moltorino failed: $Arguments ($($process.ExitCode))"
    }
}
Invoke-Package @('--moltorino-release-identity')
$identityPath = Join-Path $PackageDirectory '.moltorino-release-identity-probe.json'
$identity = Get-Content $identityPath -Raw | ConvertFrom-Json
Write-Output ($identity | ConvertTo-Json -Compress)
if ($identity.fullVersion -cne 'Moltorino M15.5.2') { throw "Incorrect displayed version: $($identity.fullVersion)" }
if ($identity.nightly -or $identity.portable) { throw 'Expected stable build using normal settings' }
$expectedSettings = Join-Path $env:APPDATA 'Chatterino2/Settings'
if ([IO.Path]::GetFullPath($identity.settingsDirectory) -ine [IO.Path]::GetFullPath($expectedSettings)) {
    throw "Unexpected settings path: $($identity.settingsDirectory); expected $expectedSettings"
}
Remove-Item $identityPath
# CI uses the real normal-profile path. These fixtures run only on fresh runners;
# refuse to overwrite a profile if the runner already has one.
$settingsFile = Join-Path $identity.settingsDirectory 'settings.json'
if (Test-Path $settingsFile) { throw "Refusing to replace an existing profile at $settingsFile" }
New-Item -ItemType Directory -Path $identity.settingsDirectory -Force | Out-Null
foreach ($testCase in @(
    @{ name = 'Baseline'; theme = 'Dark'; enabled = $false; disableGpu = $true },
    @{ name = 'Glass-Dark'; theme = 'Dark'; enabled = $false },
    @{ name = 'Glass-Theme'; theme = 'Glass'; enabled = $true },
    @{ name = 'Glass-Wallpaper'; theme = 'Dark'; enabled = $false; wallpaper = ':/themes/wallpapers/midnight-garden.jpg' }
)) {
    if ($testCase.disableGpu) { $env:MOLTORINO_GLASS_DISABLE = '1' }
    else { Remove-Item Env:MOLTORINO_GLASS_DISABLE -ErrorAction SilentlyContinue }
    if ($testCase.wallpaper) { $env:MOLTORINO_UI_REVIEW_WALLPAPER = $testCase.wallpaper }
    else { Remove-Item Env:MOLTORINO_UI_REVIEW_WALLPAPER -ErrorAction SilentlyContinue }
    $theme = $testCase.theme
    $profile = @{ appearance = @{ glassSkinEnabled = $testCase.enabled; theme = @{ name = $theme; moltorinoSelection = $theme; defaultApplied = $true; bluzyrinoMigrated = $true } } }
    $profile | ConvertTo-Json -Depth 8 | Set-Content $settingsFile -Encoding utf8
    $before = (Get-FileHash $settingsFile).Hash
    $env:MOLTORINO_UI_REVIEW_DIR = Join-Path $ReviewDirectory $testCase.name
    New-Item -ItemType Directory -Path $env:MOLTORINO_UI_REVIEW_DIR -Force | Out-Null
    $env:CHATTERINO_LOG_TO_FILE = Join-Path $env:MOLTORINO_UI_REVIEW_DIR 'startup.log'
    Invoke-Package @('--verbose')
    if ((Get-FileHash $settingsFile).Hash -ne $before) { throw "UI review modified the existing $theme settings fixture" }
    foreach ($capture in @('chat.png', 'chat-resized.png', 'menu.png', 'settings.png', 'emotes.png', 'dialog.png', 'composer.json', 'chrome.json')) {
        if (!(Test-Path (Join-Path $env:MOLTORINO_UI_REVIEW_DIR $capture))) { throw "Missing native Qt capture: $theme/$capture" }
    }
    if (!$testCase.disableGpu) {
        $gpu = Get-Content (Join-Path $env:MOLTORINO_UI_REVIEW_DIR 'chrome.json') -Raw | ConvertFrom-Json
        if (!$gpu.available -or !$gpu.qmlReady -or $gpu.graphicsApi -ne 4) { throw 'Actual Moltorino is not using the D3D11 GPU material' }
        foreach ($kind in @('tab','header','composer','button')) {
            if (!($gpu.surfaces | Where-Object { $_.kind -eq $kind })) { throw "Missing real application material surface: $kind" }
        }
        Write-Output "Real application GPU material: $($gpu.surfaces.Count) surfaces; D3D11; old plugin setting=$($testCase.enabled)"
    }
    $composer = Get-Content (Join-Path $env:MOLTORINO_UI_REVIEW_DIR 'composer.json') -Raw | ConvertFrom-Json
    Write-Output ($composer | ConvertTo-Json -Compress)
    if (!$composer.textPreserved -or $composer.multilineHeight -le $composer.singleLineHeight -or [Math]::Abs($composer.restoredHeight - $composer.singleLineHeight) -gt 2) { throw 'Composer resize/multiline regression' }
    Write-Output "Existing $theme profile loaded; settings unchanged; native widget captures saved."
}
Remove-Item Env:MOLTORINO_UI_REVIEW_DIR
Remove-Item Env:CHATTERINO_LOG_TO_FILE

Remove-Item Env:MOLTORINO_GLASS_DISABLE -ErrorAction SilentlyContinue
Remove-Item Env:MOLTORINO_UI_REVIEW_WALLPAPER -ErrorAction SilentlyContinue
