<#
TempleFrog frame-time suite. Windowed, real presents, scripted input.

Scenarios (each run twice): cold start (runtime recomp_cache cleared) and warm
start. One run replays tools/perf/make_trace.py's route from a fresh scratch
save: boot/Konami/title (f0-700), menus and file select (f700-2200), first
level traversal (f2200 on). Every presented frame's phase timing goes to a CSV
(GBARECOMP_FRAME_PHASE), plus DWM present cadence (GBARECOMP_PRESENT_CADENCE).

No build lock: other recomp chats may build at the same time, which makes the numbers noisier. Re-run a
suite if a hitch lines up with someone else's build.

  powershell -NoProfile -ExecutionPolicy Bypass -File tools\perf\run_suite.ps1 [-Label name] [-Frames 5400] [-SaveBaseline]
#>
param(
    [string]$Label = (Get-Date -Format 'yyyyMMdd-HHmmss'),
    [string]$Exe = 'build\TempleFrog.exe',   # e.g. build-static\TempleFrog.exe for a baseline
    [int]$Frames = 5400,
    [int]$Repeats = 2,
    [switch]$SaveBaseline
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$exe = if ([IO.Path]::IsPathRooted($Exe)) { $Exe } else { Join-Path $root $Exe }
$out = Join-Path $root "captures\perf\$Label"
$baseline = Join-Path $root 'tools\perf\baseline.json'
$segments = 'boot:0-700,menus:700-2200,level:2200-'
New-Item -ItemType Directory -Force $out | Out-Null
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"

$trace = Join-Path $out 'input_trace.txt'
python -I (Join-Path $PSScriptRoot 'make_trace.py') $trace --frames $Frames --walk-start 2400 | Out-Null

$csvs = @()
try {
    $runs = @()
    foreach ($kind in 'cold', 'warm') { for ($i = 1; $i -le $Repeats; $i++) { $runs += "$kind$i" } }
    foreach ($run in $runs) {
        if ($run -like 'cold*') { Remove-Item (Join-Path $root 'recomp_cache') -Recurse -Force -ErrorAction SilentlyContinue }
        $save = Join-Path $out "$run.sav"; Remove-Item $save -ErrorAction SilentlyContinue
        $csv = Join-Path $out "$run.frames.csv"
        $env:GBARECOMP_FRAME_PHASE = $csv
        $env:GBARECOMP_PRESENT_CADENCE = '1'
        $env:GBARECOMP_PRESENT_CADENCE_DUMP = Join-Path $out "$run.present.csv"
        $env:GBARECOMP_INPUT_REPLAY = $trace
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $p = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru `
            -ArgumentList @('--no-launcher', '--window', '--volume', '0', '--frames', $Frames, '--save-path', "`"$save`"", 'game.toml') `
            -RedirectStandardOutput (Join-Path $out "$run.log") -RedirectStandardError (Join-Path $out "$run.err.log")
        if (-not $p.WaitForExit(($Frames / 50 + 60) * 1000)) { $p.Kill(); Write-Host "$run TIMEOUT" }
        Write-Host ("{0}: exit={1} wall={2:n1}s csv={3}" -f $run, $p.ExitCode, $sw.Elapsed.TotalSeconds, (Test-Path $csv))
        if (Test-Path $csv) { $csvs += $csv }
    }
} finally {
    foreach ($v in 'GBARECOMP_FRAME_PHASE', 'GBARECOMP_PRESENT_CADENCE', 'GBARECOMP_PRESENT_CADENCE_DUMP', 'GBARECOMP_INPUT_REPLAY') {
        Remove-Item "Env:$v" -ErrorAction SilentlyContinue
    }
}

if (-not $csvs) { throw 'no frame CSVs captured' }
$reportArgs = @('-I', (Join-Path $PSScriptRoot 'frametime_report.py')) + $csvs + @(
    '--segments', $segments, '--skip', '30', '--json', (Join-Path $out 'report.json'), '--md', (Join-Path $out 'report.md'))
if ((Test-Path $baseline) -and -not $SaveBaseline) { $reportArgs += @('--baseline', $baseline) }
python @reportArgs
$code = $LASTEXITCODE
if ($SaveBaseline) { Copy-Item (Join-Path $out 'report.json') $baseline -Force; Write-Host "baseline saved: $baseline" }
exit $code
