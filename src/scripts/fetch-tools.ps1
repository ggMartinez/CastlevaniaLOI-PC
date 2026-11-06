# Puts every external tool and library the build uses into tools\ (ignored by git), each at a
# pinned version: the archives are checked against their SHA-256, and ps2-web-recomp is cloned at
# a fixed commit with this port's changes (src\patches\ps2-web-recomp.patch) applied on top.
# A folder already in tools\ is left alone; delete it to fetch it again. build.bat and release.bat
# run this first. None of these are needed to play: the release carries the part of llvm-mingw
# the launcher uses to build game.dll.
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$tools = Join-Path $root "tools"
$cache = Join-Path $tools "_downloads"

# Name: folder in tools\. Inner: the folder inside the archive that becomes it (empty: the archive's root).
$archives = @(
    @{ Name = "llvm-mingw"; What = "llvm-mingw 20261020 (Clang, LLD, MinGW UCRT x86_64)"; Inner = "llvm-mingw-20261020-ucrt-x86_64"
       Url = "https://github.com/mstorsjo/llvm-mingw/releases/download/20261020/llvm-mingw-20261020-ucrt-x86_64.zip"
       Sha256 = "fd5462b7f5d93b2ff981fcd3f9c7e427e47f3203c64ea91c27bcc444eebd74cd" }
    @{ Name = "cmake"; What = "CMake 4.4.5"; Inner = "cmake-4.4.5-windows-x86_64"
       Url = "https://github.com/Kitware/CMake/releases/download/v4.4.5/cmake-4.4.5-windows-x86_64.zip"
       Sha256 = "326c826486d46a23cf9d5a1c9ee3a567d427bdd02c4a34071937a2a675cff004" }
    @{ Name = "ninja"; What = "Ninja 1.13.2"; Inner = ""
       Url = "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip"
       Sha256 = "07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65" }
    @{ Name = "wgpu-native"; What = "wgpu-native v29.0.1.1 (GPU renderer: Direct3D 12 / Vulkan)"; Inner = ""
       Url = "https://github.com/gfx-rs/wgpu-native/releases/download/v29.0.1.1/wgpu-windows-x86_64-gnu-release.zip"
       Sha256 = "d471e3614733c1d4ddd61bfd19868356477d0d37bf531bf8c6cb64a7f579bd2a" }
    @{ Name = "SDL2"; What = "SDL 2.32.10 for MinGW (window, input, sound)"; Inner = "SDL2-2.32.10"
       Url = "https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.zip"
       Sha256 = "f15cff5fca62ec9381a016ef1d42a95c638cd72d2f226ba5781c76fe43dbd1ac" }
    @{ Name = "imgui"; What = "Dear ImGui v1.93.0 (the launcher's interface)"; Inner = "imgui-1.93.0"
       Url = "https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0.zip"
       Sha256 = "e21907fd8dd5ea59aa00538632a02ad0a3ededb4ddc533b94702331fbcd6c470" }
)

# snuri00/ps2-web-recomp (MIT): the PS2 recompiler (eerecomp, vurecomp) and the runtime.
$recompUrl = "https://github.com/snuri00/ps2-web-recomp.git"
$recompCommit = "e026b971324068773eed88bf502437de9e581b6b"

function Invoke-Native {
    & $args[0] @($args | Select-Object -Skip 1)
    if ($LASTEXITCODE -ne 0) { throw "$($args[0]) failed ($LASTEXITCODE): $($args -join ' ')" }
}

New-Item -ItemType Directory -Force $tools | Out-Null
$tar = Join-Path $env:SystemRoot "System32\tar.exe"   # Windows' own tar, which reads zip files

foreach ($a in $archives) {
    $dest = Join-Path $tools $a.Name
    if (Test-Path $dest) { continue }
    New-Item -ItemType Directory -Force $cache | Out-Null
    $zip = Join-Path $cache ([IO.Path]::GetFileName($a.Url))
    if (-not (Test-Path $zip) -or (Get-FileHash $zip -Algorithm SHA256).Hash -ne $a.Sha256.ToUpper()) {
        "== Downloading $($a.What)"
        Invoke-Native curl.exe -fsSL -o $zip $a.Url
    }
    $hash = (Get-FileHash $zip -Algorithm SHA256).Hash
    if ($hash -ne $a.Sha256.ToUpper()) { Remove-Item $zip; throw "$($a.Url): SHA-256 is $hash, expected $($a.Sha256)" }
    $unpack = Join-Path $cache "$($a.Name).unpack"
    if (Test-Path $unpack) { Remove-Item $unpack -Recurse -Force }
    New-Item -ItemType Directory $unpack | Out-Null
    Invoke-Native $tar -xf $zip -C $unpack
    Move-Item $(if ($a.Inner) { Join-Path $unpack $a.Inner } else { $unpack }) $dest
    if (Test-Path $unpack) { Remove-Item $unpack -Recurse -Force }
    Remove-Item $zip
}
if (Test-Path $cache) { Remove-Item $cache -Recurse -Force }

$recomp = Join-Path $tools "ps2-web-recomp"
$patch = Join-Path $root "src\patches\ps2-web-recomp.patch"
$patchHash = (Get-FileHash $patch -Algorithm SHA256).Hash
$stamp = Join-Path $recomp ".git\castlevania-patch.sha256"   # inside .git: not part of the code
if (-not (Test-Path $recomp)) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "git is needed to fetch ps2-web-recomp: https://git-scm.com/download/win" }
    "== Cloning ps2-web-recomp at $($recompCommit.Substring(0, 7)) and applying src\patches\ps2-web-recomp.patch"
    Invoke-Native git init -q $recomp
    Invoke-Native git -C $recomp config core.autocrlf false
    Invoke-Native git -C $recomp remote add origin $recompUrl
    Invoke-Native git -C $recomp fetch -q --depth 1 origin $recompCommit
    Invoke-Native git -C $recomp checkout -q FETCH_HEAD
    Invoke-Native git -C $recomp apply --whitespace=nowarn $patch
    Set-Content -Encoding ascii $stamp $patchHash
} elseif (-not (Test-Path $stamp) -or (Get-Content $stamp) -ne $patchHash) {
    throw ("src\patches\ps2-web-recomp.patch is not the one tools\ps2-web-recomp was made with. If you changed " +
           "code there, save it first (src\scripts\update-patch.ps1 overwrites the patch with it); then delete " +
           "tools\ps2-web-recomp and build again to fetch it with the current patch.")
}
