$ErrorActionPreference='Stop'
$repoPath=Split-Path $PSScriptRoot -Parent
$workPath=Join-Path (Split-Path $repoPath -Parent) 'work'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $workPath 'zig-cache'
foreach ($policyTest in @('ctstrategy','economy','match','tstrategy')) {
  & "$workPath\tooling\ziglang\zig.exe" c++ -target x86-windows-gnu -std=c++20 "-I$repoPath\inc" "$repoPath\tests\${policyTest}_standalone.cpp" -o "$workPath\$policyTest-tests.exe"
  if($LASTEXITCODE -ne 0){throw "Native $policyTest test compilation failed"}
  & "$workPath\$policyTest-tests.exe"
  if($LASTEXITCODE -ne 0){throw "Native $policyTest tests failed"}
}
