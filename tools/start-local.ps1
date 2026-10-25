param([string]$SteamHalfLife = 'C:\Program Files (x86)\Steam\steamapps\common\Half-Life')
$ErrorActionPreference = 'Stop'
$repoPath = Split-Path $PSScriptRoot -Parent
$workspacePath = Split-Path $repoPath -Parent
$workPath = Join-Path $workspacePath 'work'
$runtimePath = Join-Path $workPath 'runtime\Half-Life'
$logPath = Join-Path $workPath 'logs'
New-Item -ItemType Directory -Force $logPath | Out-Null
if (Get-Process hlds -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq "$runtimePath\hlds.exe" }) {
  throw 'The local server is already running. Stop it with tools/stop-local.ps1 before starting again.'
}
if (!(Test-Path -LiteralPath "$runtimePath\hlds.exe")) {
  if (!(Test-Path -LiteralPath "$SteamHalfLife\hlds.exe")) { throw 'Counter-Strike / HLDS installation not found' }
  New-Item -ItemType Directory -Force $runtimePath | Out-Null
  robocopy $SteamHalfLife $runtimePath /E /XD htmlcache cstrike_downloads valve_downloads cstrike_hd valve_hd /XF *.dem *.log /NFL /NDL /NJH /NJS /NP | Out-Null
  if ($LASTEXITCODE -ge 8) { throw 'Could not copy the installed game' }
}
node "$repoPath\bridge\prepare-local.mjs"
if ($LASTEXITCODE -ne 0) { throw 'Runtime preparation failed; server was not started' }
$bridgePidFile = Join-Path $workPath 'bridge.pid'
$bridgeProcess = $null
if (Test-Path -LiteralPath $bridgePidFile) {
  $recordedBridgeId = [int](Get-Content -LiteralPath $bridgePidFile)
  if ($recordedBridgeId -gt 0) { $bridgeProcess = Get-Process -Id $recordedBridgeId -ErrorAction SilentlyContinue }
  if ($bridgeProcess -and $bridgeProcess.ProcessName -ne 'node') { throw 'Stale bridge process identifier; check it before continuing' }
}
if (!$bridgeProcess) {
  $bridgeProcess = Start-Process -FilePath (Get-Command node).Source -ArgumentList '--env-file=.env','server.mjs' -WorkingDirectory "$repoPath\bridge" -WindowStyle Hidden -RedirectStandardOutput "$logPath\bridge.stdout.log" -RedirectStandardError "$logPath\bridge.stderr.log" -PassThru
  Set-Content -LiteralPath $bridgePidFile -Value $bridgeProcess.Id
}
$env:YB_SINGLE_THREADED = '1'
$serverProcess = Start-Process -FilePath "$runtimePath\hlds.exe" -ArgumentList '-console','-game','cstrike','+ip','127.0.0.1','-port','27016','-insecure','-condebug','+maxplayers','16','+map','de_dust2','+exec','jev_local.cfg' -WorkingDirectory $runtimePath -WindowStyle Hidden -RedirectStandardOutput "$logPath\hlds.stdout.log" -RedirectStandardError "$logPath\hlds.stderr.log" -PassThru
Set-Content -LiteralPath "$workPath\server.pid" -Value $serverProcess.Id
# Wait for the server to answer, rather than assuming process creation means ready.
$ready = $false
for ($attempt=0; $attempt -lt 20; $attempt++) {
  $status = & node "--env-file=$repoPath\bridge\.env" "$repoPath\bridge\control.mjs" status 2>$null
  if ($LASTEXITCODE -eq 0 -and $status -match 'map\s+:\s+de_dust2') { $ready=$true; break }
}
if (!$ready) { throw 'The server did not become ready; inspect work/logs and the game window before restarting' }
node "--env-file=$repoPath\bridge\.env" "$repoPath\bridge\seed.mjs"
if ($LASTEXITCODE -ne 0) { throw 'Could not seed the test roster' }
Write-Output 'Ready: connect to 127.0.0.1:27016 from Counter-Strike 1.6. Join CT to see team-chat assignments.'
