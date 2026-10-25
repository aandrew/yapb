$ErrorActionPreference = 'Stop'
$repoPath = Split-Path $PSScriptRoot -Parent
$workspacePath = Split-Path $repoPath -Parent
$toolsPath = Join-Path $workspacePath 'work\tooling'
$env:PATH = "$toolsPath\ziglang;$toolsPath\cmake\data\bin;$toolsPath\bin;$env:PATH"
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $workspacePath 'work\zig-cache'
& "$toolsPath\cmake\data\bin\cmake.exe" -S $repoPath -B "$repoPath\build\local" -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$repoPath/cmake/toolchains/windows-x86-clang.cmake" "-DCMAKE_AR=$workspacePath/work/zig-ar.cmd" "-DCMAKE_RANLIB=$workspacePath/work/zig-ranlib.cmd" '-DYAPB_WITH_AMXX=OFF' '-DYAPB_WITH_TLS=OFF' '-DYAPB_LTO=OFF'
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& "$toolsPath\cmake\data\bin\cmake.exe" --build "$repoPath\build\local" -j 4
if ($LASTEXITCODE -ne 0) { throw 'YaPB build failed' }
