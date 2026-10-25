$ErrorActionPreference='Stop'
$repoPath=Split-Path $PSScriptRoot -Parent
$workPath=Join-Path (Split-Path $repoPath -Parent) 'work'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $workPath 'zig-cache'
& "$workPath\tooling\ziglang\zig.exe" c++ -target x86-windows-gnu -std=c++20 "-I$repoPath\inc" "$repoPath\tests\ctstrategy_standalone.cpp" -o "$workPath\ctstrategy-tests.exe"
if($LASTEXITCODE -ne 0){throw 'Native policy test compilation failed'}
& "$workPath\ctstrategy-tests.exe"
if($LASTEXITCODE -ne 0){throw 'Native policy tests failed'}
