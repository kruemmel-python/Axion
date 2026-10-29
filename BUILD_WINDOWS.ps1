$ErrorActionPreference = 'Stop'
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
if (!(Test-Path $cmake)) { throw 'Native CMake fehlt: C:\Program Files\CMake\bin\cmake.exe' }
& $cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot 'build-windows') -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake-Konfiguration fehlgeschlagen' }
& $cmake --build (Join-Path $PSScriptRoot 'build-windows') --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'MSVC-Build fehlgeschlagen' }
& (Join-Path $PSScriptRoot 'build-windows\Release\math_research_v03.exe') selftest (Join-Path $PSScriptRoot 'build-windows\selftest-win')
if ($LASTEXITCODE -ne 0) { throw 'Selbsttest fehlgeschlagen' }
Write-Host 'BUILD UND SELBSTTEST OK'
