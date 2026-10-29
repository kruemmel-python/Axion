# Axion - Build Skript (KR Compiler / CMake)
param(
    [string]$Compiler = "H:\KR_COMPILER\KR-Portable-CPP-MultiHost_v2.4\KR-COMPILE.exe"
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  AXION – Build Prozess (Release x64)" -ForegroundColor Cyan
Write-Host "  Entwickler: Ralf Krümmel" -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan

if (Test-Path $Compiler) {
    Write-Host "[KR] Verwende portablen KR-Compiler: $Compiler" -ForegroundColor Green
    Start-Process -FilePath $Compiler -ArgumentList "--build", "`"$root`"", "--target", "WINDOWS", "--configuration", "Release" -Wait
    if (Test-Path (Join-Path $root "KR_BUILD\Release\axion_engine.exe")) {
        Write-Host "`n[ERFOLG] Axion Engine & IDE erfolgreich gebaut in KR_BUILD\Release!" -ForegroundColor Green
    }
} else {
    Write-Host "[CMake] KR-Compiler nicht unter $Compiler gefunden. Starte natives CMake..." -ForegroundColor Yellow
    $cmake = "C:\Program Files\CMake\bin\cmake.exe"
    $buildDir = Join-Path $root "build-windows"
    & $cmake -S $root -B $buildDir -G "Visual Studio 17 2022" -A x64
    & $cmake --build $buildDir --config Release --parallel
}
