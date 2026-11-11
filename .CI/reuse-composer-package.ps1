param(
    [ValidateSet('Resolve', 'Verify')][string] $Mode,
    [string] $DownloadDirectory = 'build/reusable-package',
    [string] $ReviewDirectory = 'build/lab-review/composer'
)
$ErrorActionPreference = 'Stop'
if ($Mode -eq 'Resolve') {
    $headers = @{ Authorization = "Bearer $env:GH_TOKEN"; Accept = 'application/vnd.github+json' }
    $branch = [Uri]::EscapeDataString($env:GITHUB_REF_NAME)
    $api = "https://api.github.com/repos/$env:GITHUB_REPOSITORY/actions"
    $runs = Invoke-RestMethod "$api/workflows/glass-ui-windows.yml/runs?branch=$branch&status=success&per_page=10" -Headers $headers
    foreach ($run in $runs.workflow_runs) {
        if ($run.head_repository.full_name -ne $env:GITHUB_REPOSITORY) { continue }
        $artifacts = Invoke-RestMethod "$api/runs/$($run.id)/artifacts" -Headers $headers
        $names = @($artifacts.artifacts | Where-Object { !$_.expired } | ForEach-Object { $_.name })
        if ('Moltorino-Windows' -in $names -and 'Moltorino-Build-Metadata' -in $names) {
            "run_id=$($run.id)" >> $env:GITHUB_OUTPUT
            "source_commit=$($run.head_sha)" >> $env:GITHUB_OUTPUT
            Write-Output "Selected successful full build $($run.id), commit $($run.head_sha)"
            return
        }
    }
    Write-Output '::notice::No reusable verified full build on this branch yet; application C++/composer validation requires the full workflow.'
    "Composer package check: requires a successful full build on this branch. GPU/QML checks run independently." >> $env:GITHUB_STEP_SUMMARY
    return
}

$metadata = Get-Content (Join-Path $DownloadDirectory 'metadata/build-source.json') -Raw | ConvertFrom-Json
$current = python .CI/build-fingerprint.py | ConvertFrom-Json
if ($metadata.sourceCommit -cne $env:VERIFIED_SOURCE_COMMIT -or !$metadata.lto -or $metadata.nightly -or $metadata.qtVersion -cne $env:QT_VERSION) {
    throw 'Package provenance/configuration does not match the selected successful production run'
}
if ($metadata.nativeFingerprint -cne $current.nativeFingerprint) {
    Write-Output '::notice::Native inputs changed. The cached executable cannot validate the changed composer; the full workflow is required.'
    'Composer package check: native inputs changed; full rebuild required. No stale binary was tested.' >> $env:GITHUB_STEP_SUMMARY
    return
}
$extract = Join-Path $env:RUNNER_TEMP 'fast-composer-package'
Expand-Archive (Join-Path $DownloadDirectory 'package/Moltorino-Windows.zip') $extract
$package = Join-Path $extract 'Moltorino-Glass'
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
$env:QT_PLUGIN_PATH = $package
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $package 'platforms'
& ./.CI/verify-glass-ui.ps1 -PackageDirectory $package -ReviewDirectory (Join-Path $env:GITHUB_WORKSPACE $ReviewDirectory)
if (!$?) { throw 'Reused actual composer validation failed' }
"Actual Moltorino composer: matching native fingerprint; single/multiline/narrow/restored checks passed using full run $env:VERIFIED_RUN_ID. The updated QML/QSB resources are validated separately by the standalone GPU executable." >> $env:GITHUB_STEP_SUMMARY
