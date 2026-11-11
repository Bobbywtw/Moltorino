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
foreach ($theme in @('Dark', 'Glass', 'Moltorino Midnight', 'Light')) {
    $profile = @{ appearance = @{ theme = @{ name = $theme; moltorinoSelection = $theme; defaultApplied = $true; bluzyrinoMigrated = $true } } }
    $profile | ConvertTo-Json -Depth 8 | Set-Content $settingsFile -Encoding utf8
    $before = (Get-FileHash $settingsFile).Hash
    $env:MOLTORINO_UI_REVIEW_DIR = Join-Path $ReviewDirectory $theme
    New-Item -ItemType Directory -Path $env:MOLTORINO_UI_REVIEW_DIR -Force | Out-Null
    $env:CHATTERINO_LOG_TO_FILE = Join-Path $env:MOLTORINO_UI_REVIEW_DIR 'startup.log'
    Invoke-Package @('--verbose')
    if ((Get-FileHash $settingsFile).Hash -ne $before) { throw "UI review modified the existing $theme settings fixture" }
    foreach ($capture in @('chat.png', 'menu.png', 'material-state.json')) {
        if (!(Test-Path (Join-Path $env:MOLTORINO_UI_REVIEW_DIR $capture))) { throw "Missing native Qt capture: $theme/$capture" }
    }
    $material = Get-Content (Join-Path $env:MOLTORINO_UI_REVIEW_DIR 'material-state.json') -Raw | ConvertFrom-Json
    Write-Output ($material | ConvertTo-Json -Depth 5 -Compress)
    if ($material.singleLineComposerHeight -gt 44) { throw "Oversized single-line composer: $($material.singleLineComposerHeight)" }
    if ($material.multilineComposerHeight -le $material.singleLineComposerHeight) { throw 'Composer no longer grows for multiline input' }
    foreach ($button in $material.buttons) {
        if ($button.hitWidth -gt 25 -or $button.hitHeight -gt 25 -or $button.glyphWidth -gt 17) {
            throw "Oversized composer icon: $($button | ConvertTo-Json -Compress)"
        }
    }
    Write-Output "Existing $theme profile loaded; settings unchanged; native widget captures saved."
}
Remove-Item Env:MOLTORINO_UI_REVIEW_DIR
Remove-Item Env:CHATTERINO_LOG_TO_FILE
