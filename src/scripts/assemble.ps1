# Lays out a launcher folder: the runner/launcher, its DLLs, and everything the launcher needs to
# build game.dll on a player's PC (translators, a trimmed llvm-mingw, the runtime headers, the game
# config). No game code or data goes in. Called by build.bat and release.bat.
#   -Clean  start from an empty folder (release); otherwise files are updated in place and what
#           testing left there (game.dll, game\, saves\, logs\, settings.ini) stays
#   -Pdb    also copy the .pdb debug files
param(
    [Parameter(Mandatory)][string]$Build,
    [Parameter(Mandatory)][string]$Out,
    [switch]$Clean,
    [switch]$Pdb
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$tools = Join-Path $root "tools"
$recomp = Join-Path $tools "ps2-web-recomp"
$llvm = Join-Path $tools "llvm-mingw"
$config = Join-Path $root "src\config"

function Copy-Tree($from, $to) {
    robocopy $from $to /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy $from failed ($LASTEXITCODE)" }
}

if ($Clean -and (Test-Path $Out)) { Remove-Item $Out -Recurse -Force }
$compiler = Join-Path $Out "compiler"
New-Item -ItemType Directory -Force $Out, $compiler, "$compiler\bin", "$compiler\config", "$compiler\ps2", "$Out\licenses" | Out-Null

# Runner + launcher
foreach ($f in "CastlevaniaLoI.exe", "SDL2.dll", "wgpu_native.dll") { Copy-Item (Join-Path $Build $f) $Out }
if ($Pdb) { Copy-Item (Join-Path $Build "CastlevaniaLoI.pdb") $Out }

# Translators and what game.dll is built against
Copy-Item (Join-Path $Build "eerecomp.exe"), (Join-Path $Build "vurecomp.exe") $compiler
if ($Pdb) { Copy-Item (Join-Path $Build "eerecomp.pdb"), (Join-Path $Build "vurecomp.pdb") $compiler }
Copy-Item (Join-Path $Build "libCastlevaniaLoI.dll.a") "$compiler\ps2"
Copy-Item (Join-Path $recomp "host\game_entry.cpp") "$compiler\ps2"
Copy-Tree (Join-Path $recomp "runtime\include") "$compiler\ps2\include"
Copy-Item (Join-Path $config "game.txt"), (Join-Path $config "entries.txt"), (Join-Path $config "vu1.txt"), (Join-Path $config "code-patches.txt") "$compiler\config"

# llvm-mingw, x86_64 only: the compiler driver, lld, their DLLs and config, headers, libraries
$bin = "x86_64-w64-mingw32-clang++.exe", "x86_64-w64-mingw32-clang.exe", "clang.exe", "clang++.exe", "clang-23.exe",
       "clang-target-wrapper.exe", "ld.lld.exe", "libLLVM-23.dll", "libclang-cpp.dll", "libc++.dll", "libunwind.dll",
       "mingw32-common.cfg", "x86_64-w64-windows-gnu.cfg", "x86_64-pc-windows-gnu.cfg"
foreach ($f in $bin) { $p = Join-Path "$llvm\bin" $f; if (Test-Path $p) { Copy-Item $p "$compiler\bin" } }
Copy-Tree "$llvm\include" "$compiler\include"
Copy-Tree "$llvm\x86_64-w64-mingw32\lib" "$compiler\x86_64-w64-mingw32\lib"
$clangVer = (Get-ChildItem "$llvm\lib\clang" -Directory | Select-Object -First 1).Name
Copy-Tree "$llvm\lib\clang\$clangVer\include" "$compiler\lib\clang\$clangVer\include"
New-Item -ItemType Directory -Force "$compiler\lib\clang\$clangVer\lib\windows" | Out-Null
Copy-Item "$llvm\lib\clang\$clangVer\lib\windows\libclang_rt.builtins-x86_64.a" "$compiler\lib\clang\$clangVer\lib\windows"

# Licenses of everything shipped
Copy-Item (Join-Path $recomp "LICENSE") "$Out\licenses\ps2-web-recomp.txt"
Copy-Item "$llvm\LICENSE.TXT" "$Out\licenses\llvm-mingw.txt"
Copy-Item "$tools\SDL2\LICENSE.txt" "$Out\licenses\SDL2.txt"
Copy-Item "$tools\imgui\LICENSE.txt" "$Out\licenses\dear-imgui.txt"
$wgpuLicense = Get-ChildItem "$tools\wgpu-native" -Recurse -Filter "LICENSE*" | Select-Object -First 1
if ($wgpuLicense) { Copy-Item $wgpuLicense.FullName "$Out\licenses\wgpu-native.txt" }

$size = (Get-ChildItem $Out -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
"Assembled {0} ({1:N0} MB)" -f (Resolve-Path $Out), $size
exit 0
