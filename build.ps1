param([string]$DeployTo)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products "*" -property installationPath
    $cmake = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}
$build = Join-Path $root "build\1.0.0"
& $cmake -S $root -B $build -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
& $cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { throw "Build failed." }
$ctest = Join-Path (Split-Path $cmake) "ctest.exe"
& $ctest --test-dir $build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed." }
if ($DeployTo) {
    $runtime = (Resolve-Path -LiteralPath $DeployTo).Path
    if (-not (Test-Path -LiteralPath (Join-Path $runtime "azimuth.exe"))) {
        throw "DeployTo must be an Azimuth runtime folder."
    }
    $exe = Join-Path $runtime "azimuth.exe"
    if (Get-CimInstance Win32_Process -Filter "Name = 'azimuth.exe'" |
        Where-Object { $_.ExecutablePath -eq $exe }) { throw "Close the selected runtime first." }
    $mods = Join-Path $runtime "fh2-mods"
    New-Item -ItemType Directory -Path $mods -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root "dist\DoorsUnlocked.dll") -Destination $mods -Force
    if (-not (Test-Path -LiteralPath (Join-Path $mods "DoorsUnlocked.ini"))) {
        Copy-Item -LiteralPath (Join-Path $root "DoorsUnlocked.ini") -Destination $mods
    }
}
Write-Host "Done."
