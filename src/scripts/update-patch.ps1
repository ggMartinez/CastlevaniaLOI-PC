# Writes the changes in tools\ps2-web-recomp (everything that differs from the pinned upstream
# commit, new files included) into src\patches\ps2-web-recomp.patch, so they are kept in git.
# Run it after changing the recompiler or the runtime.
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$recomp = Join-Path $root "tools\ps2-web-recomp"
$patch = Join-Path $root "src\patches\ps2-web-recomp.patch"
if (-not (Test-Path (Join-Path $recomp ".git"))) { throw "tools\ps2-web-recomp is not there: build once to fetch it" }

git -C $recomp add -A
if ($LASTEXITCODE -ne 0) { throw "git add failed" }
# --output writes the bytes as they are (a PowerShell redirect would re-encode them).
git -C $recomp diff --cached --binary HEAD "--output=$patch"
if ($LASTEXITCODE -ne 0) { throw "git diff failed" }
git -C $recomp reset -q
Set-Content -Encoding ascii (Join-Path $recomp ".git\castlevania-patch.sha256") (Get-FileHash $patch -Algorithm SHA256).Hash
$files = (Select-String -Path $patch -Pattern '^diff --git').Count
"Saved src\patches\ps2-web-recomp.patch ($files files changed from upstream)"
