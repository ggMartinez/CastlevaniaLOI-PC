# Writes the changes in tools\ps2-web-recomp (everything that differs from the pinned upstream
# commit, new files included) into src\patches\ps2-web-recomp.patch, so they are kept in git.
# Run it after changing the recompiler or the runtime.
#   -Check  writes nothing: exits with 1 when tools\ps2-web-recomp has changes the patch does not
#           hold (clean-build.bat uses it before it deletes the folder)
param([switch]$Check)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$recomp = Join-Path $root "tools\ps2-web-recomp"
$patch = Join-Path $root "src\patches\ps2-web-recomp.patch"
if (-not (Test-Path (Join-Path $recomp ".git"))) {
    if ($Check) { exit 0 }
    throw "tools\ps2-web-recomp is not there: build once to fetch it"
}

$out = if ($Check) { Join-Path $recomp ".git\castlevania-check.patch" } else { $patch }
git -C $recomp add -A
if ($LASTEXITCODE -ne 0) { throw "git add failed" }
# --output writes the bytes as they are (a PowerShell redirect would re-encode them).
git -C $recomp diff --cached --binary HEAD "--output=$out"
if ($LASTEXITCODE -ne 0) { throw "git diff failed" }
git -C $recomp reset -q

if ($Check) {
    $same = (Test-Path $patch) -and (Get-FileHash $out).Hash -eq (Get-FileHash $patch).Hash
    Remove-Item $out
    if ($same) { exit 0 }
    "tools\ps2-web-recomp has changes that src\patches\ps2-web-recomp.patch does not hold."
    exit 1
}
Set-Content -Encoding ascii (Join-Path $recomp ".git\castlevania-patch.sha256") (Get-FileHash $patch -Algorithm SHA256).Hash
$files = (Select-String -Path $patch -Pattern '^diff --git').Count
"Saved src\patches\ps2-web-recomp.patch ($files files changed from upstream)"
