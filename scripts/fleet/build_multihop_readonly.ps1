<#
  Three-vessel multihop feature-branch preflight.
  Compiles host tests and PlatformIO profiles; NEVER flashes devices or edits Git.
  Run from an isolated worktree descended from the verified integration commit.
#>
param(
    [Parameter(Mandatory=$true)][string]$RepoRoot,
    [string]$OutputDirectory = "",
    [string]$PlatformIOExe = ""
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RequiredAncestor = 'a4c84c2d47fdee795bc94bc4bbcfa65e1d3f0d35'
if (-not (Test-Path -LiteralPath $RepoRoot -PathType Container)) {
    throw "STOP: Repository directory not found: $RepoRoot"
}
$actualHead = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'STOP: Cannot read local Git HEAD.' }
& git -C $RepoRoot merge-base --is-ancestor $RequiredAncestor HEAD
if ($LASTEXITCODE -ne 0) {
    throw "STOP: Expected feature worktree descended from $RequiredAncestor; actual $actualHead"
}
$changes = @(& git -C $RepoRoot status --porcelain=v1 -uall)
if ($LASTEXITCODE -ne 0 -or $changes.Count -ne 0) {
    throw "STOP: Worktree is not clean; no build performed. $($changes -join ', ')"
}

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $env:TEMP ('THESIS_multihop_build_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$firmware = Join-Path $RepoRoot 'firmware\universal'
$testRoot = Join-Path $RepoRoot 'tests'
if (-not (Test-Path -LiteralPath (Join-Path $firmware 'platformio.ini'))) {
    throw 'STOP: Missing firmware/universal/platformio.ini'
}
Write-Host "=== THESIS FEATURE BRANCH BUILD-ONLY VALIDATION ==="
Write-Host "Commit: $actualHead"
Write-Host "Output: $OutputDirectory"
Write-Host "No upload/flash, COM, NVS or SD operations are performed."

$compiler = Get-Command g++ -ErrorAction SilentlyContinue
if ($compiler) {
    foreach ($test in @('compact_position_relay_host', 'compact_position_hop1_host', 'multihop_dev_test_host', 'multihop_supervised_bench_host', 'multihop_priority_host', 'fleet_home_title_host', 'gnss_pht_clock_host', 'multihop_deferred_radio_host', 'dtn_sync_checkpoint_host', 'dtn_jsonl_snapshot_host', 'dtn_f06_record_gate_host')) {
        $source = Join-Path $testRoot ($test + '.cpp')
        $exe = Join-Path $OutputDirectory ($test + '.exe')
        $log = Join-Path $OutputDirectory ($test + '.txt')
        if (-not (Test-Path -LiteralPath $source)) { throw "STOP: Missing host test $source" }
        & $compiler.Source -std=c++17 -Wall -Wextra -Werror $source -o $exe 2>&1 | Tee-Object -FilePath $log
        if ($LASTEXITCODE -ne 0) { throw "STOP: Host compilation failed for $test" }
        & $exe 2>&1 | Tee-Object -FilePath $log -Append
        if ($LASTEXITCODE -ne 0) { throw "STOP: Host test failed for $test" }
    }
} else {
    Write-Warning 'g++ not on PATH: host tests SKIPPED, not counted as PASS.'
}

# Prefer an explicit CLI path. PlatformIO can be installed outside USERPROFILE,
# and the executable may be named platformio.exe rather than pio.exe.
$pioExe = $null
if ($PlatformIOExe) {
    if (-not (Test-Path -LiteralPath $PlatformIOExe -PathType Leaf)) {
        throw "STOP: Specified PlatformIO executable not found: $PlatformIOExe"
    }
    $pioExe = $PlatformIOExe
} else {
    $candidates = @()
    foreach ($name in @('pio.exe','platformio.exe','pio','platformio')) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { $candidates += $cmd.Source }
    }
    $candidates += @(
        'C:\PlatformIO\penv\Scripts\platformio.exe',
        'C:\PlatformIO\penv\Scripts\pio.exe',
        (Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'),
        (Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $pioExe = $candidate
            break
        }
    }
}
if (-not $pioExe) {
    throw 'STOP: PlatformIO not found. Pass -PlatformIOExe with the full path to platformio.exe.'
}
Write-Host "PlatformIO executable: $pioExe"
& $pioExe --version
if ($LASTEXITCODE -ne 0) { throw 'STOP: PlatformIO executable failed its version check.' }
$profiles = @(
    'tbeam-supreme-fleet-universal-operational-build-only',
    'tbeam-supreme-multihop-dev-preview-build-only',
    'tbeam-supreme-multihop-v002-supervised-bench',
    'tbeam-supreme-f23d-v001-sd-evidence-build-only',
    'tbeam-supreme-f23d-v003-sd-evidence-build-only',
    'tbeam-supreme-f23d-v002-physical-sd-candidate-build-only'
)
foreach ($profile in $profiles) {
    $log = Join-Path $OutputDirectory ($profile + '.txt')
    Write-Host "Building $profile ..."
    & $pioExe run -d $firmware -e $profile 2>&1 | Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0) { throw "STOP: PlatformIO build failed for $profile; see $log" }
}
$after = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($after -ne $actualHead) { throw 'STOP: Git HEAD changed during validation.' }
Write-Host '=== SIX FIRMWARE BUILDS COMPLETED (host checks run if g++ available) ==='
Write-Host "Logs: $OutputDirectory"
Write-Host 'Passing builds are NOT flash approval. The supervised bench profile can transmit only after deliberate V002 arming; no physical multihop acceptance is implied.'
