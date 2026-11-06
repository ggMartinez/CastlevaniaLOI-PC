# Developer tool (replaces port.sh's exploring step, Windows only): runs the game headless,
# collects the code addresses it reached that the static pass missed, adds them to
# src\config\entries.txt, translates again and rebuilds, until a run finds nothing new. The VU1
# microcode the runs record is turned into src\config\vu1.txt. Those two files are what the
# launcher ships; the recorded images, the translated code and the disc's executable stay in
# output\explore (ignored by git: they are the game's).
#   src\scripts\explore.ps1 [-Frames 3600] [-Rounds 8] [-Press "300:start","600:cross",...]
param(
    [int]$Frames = 3600,
    [int]$Rounds = 8,
    [string[]]$Press = @(),
    [string]$Iso = (Join-Path $PSScriptRoot "..\..\Castlevania Lament of Innocence.iso")
)
# "Continue": Windows PowerShell turns a native program's stderr lines into errors when they are
# redirected, so failures are checked through $LASTEXITCODE instead.
$ErrorActionPreference = "Continue"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$build = Join-Path $root "output\build\debug"
$config = Join-Path $root "src\config"
$entries = Join-Path $config "entries.txt"
$dev = Join-Path $root "output\explore"
$generated = Join-Path $dev "generated"
$runner = Join-Path $build "CastlevaniaLoI.exe"
$boot = ((Get-Content (Join-Path $config "game.txt")) -match '^boot=' -replace '^boot=', '').Trim()
$elf = Join-Path $dev $boot
New-Item -ItemType Directory -Force $dev, "$dev\vu1", "$dev\shots", $generated | Out-Null

function Build-Dev {
    cmd /c "`"$PSScriptRoot\compile.cmd`" debug > `"$dev\build.log`" 2>&1"
    if ($LASTEXITCODE -ne 0) { Get-Content "$dev\build.log" -Tail 30; throw "build failed (output\explore\build.log)" }
}
function Translate {
    & "$build\eerecomp.exe" $elf --entries $entries --out $generated 2>&1 | ForEach-Object { "$_" } | Select-Object -Last 1
    if ($LASTEXITCODE -ne 0) { throw "eerecomp failed" }
}
function Read-Tokens($file) {
    if (-not (Test-Path $file)) { return @() }
    (Get-Content $file -Raw) -split '\s+' | Where-Object { $_ -and -not $_.StartsWith('#') } | ForEach-Object {
        $name, $addr = if ($_.Contains(':')) { $_.Split(':', 2) } else { '', $_ }
        $hex = '{0:x}' -f [Convert]::ToUInt32($addr, 16)
        if ($name) { "${name}:$hex" } else { $hex }
    }
}

Build-Dev
if (-not (Test-Path $elf)) {
    & $runner --dev-extract (Join-Path $config "game.txt") $Iso $dev 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { throw "cannot take $boot off the disc" }
}

# The runtime only records microcode it has no compiled version of, and vurecomp rebuilds its table
# from whatever is in the folder, so the folder starts with every known image, rebuilt from the recipe.
$elfBytes = [IO.File]::ReadAllBytes($elf)
foreach ($line in Get-Content (Join-Path $config "vu1.txt")) {
    if (-not $line.Trim() -or $line.StartsWith('#')) { continue }
    $words = $line.Trim() -split '\s+'
    $image = New-Object byte[] 16384
    foreach ($chunk in $words[1..($words.Count - 1)]) {
        $vu, $at, $size = $chunk.Split(':') | ForEach-Object { [Convert]::ToInt32($_, 16) }
        [Array]::Copy($elfBytes, $at, $image, $vu, $size)
    }
    [IO.File]::WriteAllBytes((Join-Path "$dev\vu1" $words[0]), $image)
}

if ($Press.Count -eq 0) {
    $Press = @("300:start") + (@(600..($Frames - 1)) | Where-Object { $_ % 300 -eq 0 } | ForEach-Object { "${_}:cross" })
}
$pressArgs = $Press | ForEach-Object { "--press", $_ }

"== translating $boot"
Translate
for ($round = 1; $round -le $Rounds; $round++) {
    Build-Dev
    Remove-Item "$dev\misses.txt", "$dev\explore.ps2" -ErrorAction SilentlyContinue
    Remove-Item "$dev\shots\*" -ErrorAction SilentlyContinue
    $env:PS2_VU_RECORD = "$dev\vu1"
    $env:PS2_GPU_LOW_POWER = "1"
    # The runner is a windowed program: piping its output is what makes PowerShell wait for it.
    & $runner $elf --disc $Iso --misses "$dev\misses.txt" --memcard "$dev\explore.ps2" --frames $Frames @pressArgs `
        --frames-dir "$dev\shots" --frame-every 300 2>&1 | ForEach-Object { "$_" } | Set-Content "$dev\explore.log"
    Remove-Item Env:PS2_VU_RECORD, Env:PS2_GPU_LOW_POWER
    $halt = Select-String -Path "$dev\explore.log" -Pattern '^\[ee\] halted: (.*)' | Select-Object -First 1
    $reason = if ($halt) { $halt.Matches[0].Groups[1].Value } else { "" }
    $known = @(Read-Tokens $entries)
    $new = @(Read-Tokens "$dev\misses.txt" | Where-Object { $known -notcontains $_ } | Select-Object -Unique)
    "round ${round}: $(if ($reason) { $reason } else { 'ran to the end' }), $($new.Count) new entry points"
    if ($new.Count -eq 0) { break }
    Add-Content $entries ($new -join "`n")
    Translate
}

"== vector unit programs"
if (Get-ChildItem "$dev\vu1\*.bin" -ErrorAction SilentlyContinue) {
    & "$build\vurecomp.exe" "$dev\vu1" $generated 2>&1 | ForEach-Object { "$_" } | Select-Object -Last 1
    & $runner --dev-vu-recipe $elf "$dev\vu1" (Join-Path $config "vu1.txt") 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { throw "VU recipe failed" }
}
Build-Dev
"Done: src\config\entries.txt and vu1.txt updated; frames of the last run in output\explore\shots, its log in output\explore\explore.log"
