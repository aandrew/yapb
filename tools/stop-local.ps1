$ErrorActionPreference = 'Stop'
$repoPath = Split-Path $PSScriptRoot -Parent
$workPath = Join-Path (Split-Path $repoPath -Parent) 'work'
$runtimePath = Join-Path $workPath 'runtime\Half-Life'
$servers = Get-Process hlds -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq "$runtimePath\hlds.exe" }
if ($servers) {
  & node "--env-file=$repoPath\bridge\.env" "$repoPath\bridge\control.mjs" quit 2>$null | Out-Null
  foreach ($server in $servers) {
    Wait-Process -Id $server.Id -Timeout 8 -ErrorAction SilentlyContinue
    if (Get-Process -Id $server.Id -ErrorAction SilentlyContinue) {
      Stop-Process -Id $server.Id -Force
      Wait-Process -Id $server.Id -ErrorAction SilentlyContinue
    }
  }
}
$pidFile=Join-Path $workPath 'bridge.pid'
if(Test-Path -LiteralPath $pidFile){
  $bridgeId=[int](Get-Content -LiteralPath $pidFile)
  $bridge=$null
  if($bridgeId -gt 0){ $bridge=Get-Process -Id $bridgeId -ErrorAction SilentlyContinue }
  if($bridge -and $bridge.ProcessName -eq 'node'){
    Stop-Process -Id $bridgeId -Force
    Wait-Process -Id $bridgeId -ErrorAction SilentlyContinue
  }
  Set-Content -LiteralPath $pidFile -Value '0'
}
Write-Output 'Stopped the local development server and bridge.'
