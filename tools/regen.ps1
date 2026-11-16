param(
    [string]$Rom = (Join-Path $PSScriptRoot '..\roms\frog_us.gba'),
    [string]$GbarecompRoot = (Join-Path $PSScriptRoot '..\gbarecomp'),
    [int]$MaxFunctions = 65536
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$romPath = (Resolve-Path -LiteralPath $Rom).Path
$engine = (Resolve-Path $GbarecompRoot).Path
$tool = Join-Path $engine 'build\gba_recompile.exe'
if (-not (Test-Path -LiteralPath $tool)) {
    throw "Missing $tool. Build the engine's gba_recompile target first."
}

$actual = (Get-FileHash -LiteralPath $romPath -Algorithm SHA1).Hash.ToLowerInvariant()
$expected = '7b4c27009198df18555e63fb5dcad223eaf09815'
if ($actual -ne $expected) {
    throw "Frogger's Adventures ROM SHA-1 mismatch: got $actual expected $expected"
}

# Generated BIOS (LLE). Needs the user's own BIOS at gbarecomp\bios\gba_bios.bin.
$bios = Join-Path $engine 'bios\gba_bios.bin'
$biosOut = Join-Path $engine 'src\runtime\generated_bios'
if ((Test-Path -LiteralPath $bios) -and -not (Test-Path (Join-Path $biosOut 'bios_recompiled.cpp'))) {
    & $tool --bios $bios --config (Join-Path $engine 'bios\gba_bios.toml') --out $biosOut
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $tool --rom $romPath --config (Join-Path $root 'game.toml') `
    --out (Join-Path $root 'generated') --max-functions $MaxFunctions
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
