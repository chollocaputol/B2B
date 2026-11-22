# Windows-only, local HTTP handler acceptance. No Docker or cloud calls.
param([string]$RepoRoot = "C:\Users\Public\Thesis\SYSTEM_multihop")
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

foreach ($command in @("node","git")) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "STOP: $command not on PATH."
    }
}
if (-not (Test-Path -LiteralPath $RepoRoot -PathType Container)) {
    throw "STOP: Feature worktree path does not exist."
}
$root = (Resolve-Path -LiteralPath $RepoRoot).Path
$top = (& git -C $root rev-parse --show-toplevel)
if ($LASTEXITCODE -ne 0 -or [IO.Path]::GetFullPath($top.Trim()) -ne [IO.Path]::GetFullPath($root)) {
    throw "STOP: Not the expected Git worktree root."
}
$dirty = @(& git -C $root status --porcelain=v1 -uall)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -ne 0) {
    throw "STOP: Worktree is not clean."
}
$major = [int]((& node -p "process.versions.node").Split('.')[0])
if ($LASTEXITCODE -ne 0 -or $major -lt 22) {
    throw "STOP: Node 22+ is required."
}
$tests = @(
    (Join-Path $root "tests\dtn_f06_history_validator.test.mjs"),
    (Join-Path $root "tests\dtn_f06_history_http.test.mjs")
)
foreach ($file in $tests) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "STOP: Missing test: $file"
    }
}
Write-Host "TESTING: Node HTTP loopback, mocked auth/RPC; NO SUPABASE CALLS"
& node --experimental-strip-types --test $tests
if ($LASTEXITCODE -ne 0) {
    throw "STOP: F06 history backend HTTP tests failed."
}
Write-Host "=== LOCAL F06 HISTORY HTTP GATE PASS ===" -ForegroundColor Green
