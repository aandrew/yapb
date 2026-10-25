# Downloads portable build tools into the parent workspace; no system-wide install.
$ErrorActionPreference = 'Stop'
$repoPath = Split-Path $PSScriptRoot -Parent
$workspacePath = Split-Path $repoPath -Parent
$workPath = Join-Path $workspacePath 'work'
New-Item -ItemType Directory -Force $workPath | Out-Null
python -m pip install --target "$workPath\tooling" 'cmake==4.4.4' 'ninja==1.13.2' 'ziglang==0.16.0'
if ($LASTEXITCODE -ne 0) { throw 'Portable tool download failed' }
Set-Content -LiteralPath "$workPath\zig-ar.cmd" -Value '@echo off', '"%~dp0tooling\ziglang\zig.exe" ar %*' -Encoding ascii
Set-Content -LiteralPath "$workPath\zig-ranlib.cmd" -Value '@echo off', '"%~dp0tooling\ziglang\zig.exe" ranlib %*' -Encoding ascii
New-Item -ItemType Directory -Force "$workPath\assets" | Out-Null
Invoke-WebRequest 'https://raw.githubusercontent.com/yapb/graph/master/graph/de_dust2.graph' -OutFile "$workPath\assets\de_dust2.graph"
$localEnv = Join-Path $repoPath 'bridge\.env'
if (!(Test-Path -LiteralPath $localEnv)) { Copy-Item -LiteralPath "$repoPath\bridge\.env.example" -Destination $localEnv }
Write-Output 'Portable tools and Dust2 graph are ready. Add your OpenRouter key to bridge/.env.'
