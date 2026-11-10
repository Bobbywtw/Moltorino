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
    if ($process.ExitCode -ne 0) { throw "Moltorino failed: $Arguments ($($process.ExitCode))" }
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
foreach ($theme in @('Dark', 'Glass')) {
    $profile = @{ appearance = @{ theme = @{ name = $theme; moltorinoSelection = $theme; defaultApplied = $true; bluzyrinoMigrated = $true } } }
    $profile | ConvertTo-Json -Depth 8 | Set-Content $settingsFile -Encoding utf8
    $before = (Get-FileHash $settingsFile).Hash
    $env:MOLTORINO_UI_REVIEW_DIR = Join-Path $ReviewDirectory $theme
    Invoke-Package @('--verbose')
    if ((Get-FileHash $settingsFile).Hash -ne $before) { throw "UI review modified the existing $theme settings fixture" }
    foreach ($capture in @('chat.png', 'menu.png')) {
        if (!(Test-Path (Join-Path $env:MOLTORINO_UI_REVIEW_DIR $capture))) { throw "Missing native Qt capture: $theme/$capture" }
    }
    Write-Output "Existing $theme profile loaded; settings unchanged; native widget captures saved."
}
Remove-Item Env:MOLTORINO_UI_REVIEW_DIR
