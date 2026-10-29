
# MathResearchEngine - Clean Rebuild Windows 11
$ErrorActionPreference = "Stop"

$root = "F:\MRE_Autonomous_Research_GeminiAPI"
$cmake = "C:\Program Files\CMake\bin\cmake.exe"

Set-Location $root

if (!(Test-Path $cmake)) {
    throw "CMake wurde nicht gefunden."
}

# Laufende Programme pruefen
$running = Get-Process -Name "math_research_v03", "MRE_Research_GUI", "research_candidate" -ErrorAction SilentlyContinue
if ($running) {
    throw "Bitte zuerst die Forschungsengine und GUI schliessen."
}

function Build-Project {
    param(
        [string]$Source,
        [string]$Build,
        [string]$Target
    )

    Write-Host "`n=== BUILD: $Target ===" -ForegroundColor Cyan

    & $cmake -S $Source -B $Build `
        -G "Visual Studio 17 2022" -A x64

    if ($LASTEXITCODE -ne 0) {
        throw "CMake-Konfiguration fehlgeschlagen: $Target"
    }

    & $cmake --build $Build `
        --config Release `
        --target $Target `
        --parallel 4

    if ($LASTEXITCODE -ne 0) {
        throw "Build fehlgeschlagen: $Target"
    }

    Write-Host "$Target erfolgreich gebaut." -ForegroundColor Green
}

Write-Host "`n=== CLEAN REBUILD MATHRESEARCHENGINE ===" -ForegroundColor Yellow

# Nur Build-Verzeichnisse entfernen
$buildDirs = @(
    "$root\build-windows",
    "$root\mre-gui\build-windows",
    "$root\gemini-spectral-validated\build-windows"
)

foreach ($dir in $buildDirs) {
    if (Test-Path $dir) {
        Write-Host "Entferne $dir"
        Remove-Item $dir -Recurse -Force
    }
}

# Hauptcontroller
Build-Project `
    -Source $root `
    -Build "$root\build-windows" `
    -Target "math_research_v03"

# Windows-GUI
Build-Project `
    -Source "$root\mre-gui" `
    -Build "$root\mre-gui\build-windows" `
    -Target "MRE_Research_GUI"

# Spektralanalyse
Build-Project `
    -Source "$root\gemini-spectral-validated" `
    -Build "$root\gemini-spectral-validated\build-windows" `
    -Target "research_candidate"

Write-Host "`n=== ALLE BUILDS ERFOLGREICH ===" -ForegroundColor Green

Write-Host "Starte MathResearchEngine GUI..."

$gui = "$root\mre-gui\build-windows\Release\MRE_Research_GUI.exe"

if (!(Test-Path $gui)) {
    throw "GUI-EXE wurde nicht gefunden."
}

Start-Process -FilePath $gui -WorkingDirectory $root
