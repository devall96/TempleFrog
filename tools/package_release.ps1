<#
Build TempleFrog's Windows release package from build\ (Release):

  powershell -NoProfile -ExecutionPolicy Bypass -File tools\package_release.ps1 -Version 0.1.0

Writes out\release\TempleFrog-<version>-windows-x64.zip, SHA256SUMS.txt and BUILDINFO.json.
The package holds the program, its DLLs, TempleFrog's own art and fonts, the preloaded mods and the licences.
It never holds a ROM, BIOS, save, save state, the player's settings, logs, or images made from the ROM; the audit
at the end fails the build if any of those, or a personal path, gets in.
#>
param([Parameter(Mandatory)][string]$Version)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $root 'build'
$name = "TempleFrog-$Version-windows-x64"
$out = Join-Path $root 'out\release'
$stage = Join-Path $out $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# Program and runtime DLLs.
foreach ($f in 'TempleFrog.exe', 'SDL2.dll', 'gbarecomp-hqx.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll') {
    Copy-Item (Join-Path $build $f) $stage
}
# Art and fonts (never the box art or backdrop: those are made from the player's ROM on their PC).
Copy-Item (Join-Path $build 'assets') (Join-Path $stage 'assets') -Recurse
Remove-Item (Join-Path $stage 'assets\img\boxart.tga'), (Join-Path $stage 'assets\img\backdrop.tga') -ErrorAction SilentlyContinue
Copy-Item (Join-Path $build 'third-party') (Join-Path $stage 'third-party') -Recurse
# Preloaded mod packages only (not the player's selection).
New-Item -ItemType Directory -Force (Join-Path $stage 'mods') | Out-Null
Copy-Item (Join-Path $build 'mods\packages') (Join-Path $stage 'mods\packages') -Recurse

# game.toml with release-relative BIOS path.
$toml = Get-Content (Join-Path $root 'game.toml') -Raw
$toml = $toml -replace 'path = "gbarecomp/bios/gba_bios.bin"', 'path = "bios/gba_bios.bin"'
Set-Content -Path (Join-Path $stage 'game.toml') -Value $toml -NoNewline -Encoding utf8
New-Item -ItemType Directory -Force (Join-Path $stage 'roms'), (Join-Path $stage 'bios'), (Join-Path $stage 'saves') | Out-Null
Set-Content (Join-Path $stage 'roms\PUT YOUR ROM HERE.txt') "Copy your own Frogger's Adventures: Temple of the Frog (USA) ROM here as frog_us.gba,`r`nor pick it in the launcher (Play > Browse For ROM). SHA-1: 7b4c27009198df18555e63fb5dcad223eaf09815" -Encoding ascii
Set-Content (Join-Path $stage 'bios\PUT YOUR BIOS HERE.txt') "Copy your own Game Boy Advance BIOS dump here as gba_bios.bin,`r`nor pick it in the launcher (Settings > System > BIOS). SHA-1: 300c20df6731a33952ded8c436f7f186d25d3492" -Encoding ascii

# Licences.
$lic = Join-Path $stage 'licenses'
New-Item -ItemType Directory -Force $lic | Out-Null
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $lic 'TempleFrog-MIT.txt')
Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.md') $stage
Copy-Item (Join-Path $root 'gbarecomp\LICENSE') (Join-Path $lic 'gbarecomp-PolyForm-Noncommercial-1.0.0.txt')
Copy-Item (Join-Path $root 'recomp-ui\LICENSE') (Join-Path $lic 'recomp-ui-MIT.txt')
Copy-Item 'C:\msys64\mingw64\share\licenses\SDL2\LICENSE.txt' (Join-Path $lic 'SDL2-zlib.txt')
Copy-Item 'C:\msys64\mingw64\share\licenses\gcc\COPYING.RUNTIME' (Join-Path $lic 'GCC-Runtime-Library-Exception.txt')
Copy-Item 'C:\msys64\mingw64\share\licenses\gcc\COPYING3' (Join-Path $lic 'GPL-3.0.txt')
Copy-Item 'C:\msys64\mingw64\share\licenses\libwinpthread\COPYING' (Join-Path $lic 'winpthreads.txt')
Copy-Item (Join-Path $root 'third_party\fonts\Chewy\LICENSE.txt') (Join-Path $lic 'Chewy-Apache-2.0.txt')
Copy-Item (Join-Path $root 'third_party\fonts\Fredoka\OFL.txt') (Join-Path $lic 'Fredoka-OFL-1.1.txt')
Copy-Item (Join-Path $root 'recomp-ui\assets\common\fonts\NOTICE.md') (Join-Path $lic 'Launcher-fonts-NOTICE.md')
Copy-Item (Join-Path $root 'recomp-ui\src\third_party\imgui\LICENSE.txt') (Join-Path $lic 'DearImGui-MIT.txt') -ErrorAction SilentlyContinue
Copy-Item (Join-Path $root 'docs\README-release.txt') (Join-Path $stage 'README.txt')
# RecompLauncher's setup and settings profile, next to the program (https://recomplauncher.com/sdk).
Copy-Item (Join-Path $root 'recomplauncher.json') $stage
$null = Get-Content (Join-Path $stage 'recomplauncher.json') -Raw | ConvertFrom-Json   # fails the build on bad JSON

# ---- audit ----------------------------------------------------------------
$bad = @()
$banned = '\.(gba|agb|bin|sav|srm|state\d*|cfg|ini|replay|log)$'
Get-ChildItem $stage -Recurse -File | ForEach-Object {
    if ($_.Name -match $banned) { $bad += "banned file type: $($_.FullName)" }
    if ($_.Name -in 'boxart.tga', 'backdrop.tga', 'state.toml', 'templefrog.ini') { $bad += "player or ROM-derived file: $($_.FullName)" }
}
# The Windows account name inside a user path (the public GitHub handle devall96 is fine), local folders and
# private contact details.
$needles = @('proton.me', 'OneDrive\Documents', 'Claude Work', 'devall96@', "Users\$env:USERNAME", "Users/$env:USERNAME")
Get-ChildItem $stage -Recurse -File | ForEach-Object {
    $bytes = [IO.File]::ReadAllBytes($_.FullName)
    $text = [Text.Encoding]::GetEncoding(28591).GetString($bytes)
    $wide = [Text.Encoding]::Unicode.GetString($bytes)
    foreach ($n in $needles) {
        if ($n -and ($text.IndexOf($n, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
                     $wide.IndexOf($n, [StringComparison]::OrdinalIgnoreCase) -ge 0)) {
            $bad += "personal string '$n' in $($_.FullName)"
        }
    }
}
if ($bad) { $bad | ForEach-Object { Write-Host "AUDIT FAIL: $_" }; throw 'release audit failed' }
Write-Host 'audit: clean'

# ---- zip, sums, build info ----------------------------------------------------
$zip = Join-Path $out "$name.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path $stage -DestinationPath $zip
$commit = { param($dir) (git -C $dir rev-parse HEAD).Trim() }
$info = [ordered]@{
    name = 'TempleFrog'; version = $Version; platform = 'windows-x64'
    built_utc = (Get-Date).ToUniversalTime().ToString('o')
    templefrog_commit = & $commit $root
    gbarecomp_commit = & $commit (Join-Path $root 'gbarecomp')
    recomp_ui_commit = & $commit (Join-Path $root 'recomp-ui')
    rom_sha1 = '7b4c27009198df18555e63fb5dcad223eaf09815'
    bios_sha1 = '300c20df6731a33952ded8c436f7f186d25d3492'
    toolchain = 'MSYS2 MinGW-w64 GCC, CMake Release (-O3)'
}
$info | ConvertTo-Json | Set-Content (Join-Path $out 'BUILDINFO.json') -Encoding utf8
$sums = foreach ($f in (Get-Item $zip), (Get-Item (Join-Path $out 'BUILDINFO.json'))) {
    '{0}  {1}' -f (Get-FileHash $f.FullName -Algorithm SHA256).Hash.ToLower(), $f.Name
}
Set-Content (Join-Path $out 'SHA256SUMS.txt') $sums -Encoding ascii
Get-ChildItem $out -File | Select-Object Name, Length
