# Safe, disposable PostgreSQL-only F06 archive integration tests.
# NO Supabase connection, NO published ports, NO firmware or SD access.
param(
  [string]$RepoRoot = "C:\Users\Public\Thesis\SYSTEM_multihop",
  [switch]$PreflightOnly
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Assert-Native([string]$Label) {
  if ($LASTEXITCODE -ne 0) { throw "STOP: $Label (exit $LASTEXITCODE)" }
}

foreach ($cmd in @("git", "node", "docker")) {
  if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
    throw "STOP: $cmd not found. Install/configure locally before continuing."
  }
}
if (-not (Test-Path -LiteralPath $RepoRoot -PathType Container)) {
  throw "STOP: Local feature worktree not found: $RepoRoot"
}
$resolved = (Resolve-Path -LiteralPath $RepoRoot).Path
$gitTop = (& git -C $resolved rev-parse --show-toplevel)
Assert-Native "Git worktree lookup"
if ([IO.Path]::GetFullPath($gitTop.Trim()) -ne [IO.Path]::GetFullPath($resolved)) {
  throw "STOP: RepoRoot must be the exact root of the worktree."
}
$origin = (& git -C $resolved remote get-url origin)
Assert-Native "Git remote check"
if ($origin.Trim() -notmatch '(?i)(^|[/:])chollocaputol/B2B(\.git)?$') {
  throw "STOP: Unexpected Git origin; refusing to run."
}
$dirty = @(& git -C $resolved status --porcelain=v1 -uall)
Assert-Native "Git clean worktree check"
if ($dirty.Count -gt 0) { throw "STOP: Worktree is not clean." }
$version = & node -p "process.versions.node"
Assert-Native "Node.js version check"
if ([int]($version.Split('.')[0]) -lt 22) {
  throw "STOP: Node.js 22 or newer required."
}
$serverVersion = & docker version --format '{{.Server.Version}}'
Assert-Native "Docker Desktop engine is unavailable or not running"
Write-Host "LOCAL PREFLIGHT PASS: Git + Node $version + Docker $serverVersion"
Write-Host "No cloud services or live databases will be contacted."
if ($PreflightOnly) { return }

$paths = @(
  "tests\dtn_f06_history_validator.test.mjs",
  "tests\sql\f06_archive_mock_supabase.sql",
  "supabase\migrations\20261024081011_initial_banca_monitoring_schema.sql",
  "backend\sql\proposals\f06_archive_staging.sql",
  "tests\sql\f06_archive_local_assertions.sql"
)
foreach ($relative in $paths) {
  $full = Join-Path $resolved $relative
  if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
    throw "STOP: Missing required file: $relative. Fetch latest feature commit."
  }
}
& node --experimental-strip-types --test (Join-Path $resolved $paths[0])
Assert-Native "F06 JSONL host validation"
Write-Host "JSONL validator PASS"

$container = "thesis-f06-local-" + [guid]::NewGuid().ToString("N").Substring(0,12)
$dbname = "thesis_f06_test"
$password = [guid]::NewGuid().ToString("N") + [guid]::NewGuid().ToString("N")
$started = $false
try {
  # No TCP port mapping and --network none: this container cannot reach cloud.
  & docker run -d --rm --pull=missing --network none --name $container -e "POSTGRES_PASSWORD=$password" -e "POSTGRES_DB=$dbname" postgres:17-bookworm
  Assert-Native "Disposable Docker Postgres startup"
  $started = $true

  $ready = $false
  for ($attempt = 0; $attempt -lt 60; $attempt++) {
    & docker exec $container pg_isready -U postgres -d $dbname *> $null
    if ($LASTEXITCODE -eq 0) { $ready = $true; break }
    Start-Sleep -Seconds 1
  }
  if (-not $ready) { throw "STOP: Disposable local Postgres did not become ready." }
  Start-Sleep -Seconds 2

  function Invoke-F06Sql([string]$file) {
    $path = Join-Path $resolved $file
    Write-Host "LOCAL SQL: $file"
    Get-Content -LiteralPath $path -Encoding UTF8 -Raw | & docker exec -i $container psql -X -U postgres -d $dbname --set ON_ERROR_STOP=1 -f -
    Assert-Native "Local SQL failed: $file"
  }
  # Exact real repository migration, then the UNDEPLOYED archive proposal.
  Invoke-F06Sql $paths[1]
  Invoke-F06Sql $paths[2]
  Invoke-F06Sql $paths[3]
  Invoke-F06Sql $paths[4]
  Write-Host "=== LOCAL F06 ARCHIVE DATABASE TEST PASS ===" -ForegroundColor Green
  Write-Host "No live Supabase table, device, SD log or Git file was modified."
}
finally {
  if ($started) {
    Write-Host "Removing only this test container: $container"
    & docker rm -f $container *> $null
    if ($LASTEXITCODE -ne 0) {
      Write-Warning "Manual cleanup required for disposable container: $container"
    }
  }
}
