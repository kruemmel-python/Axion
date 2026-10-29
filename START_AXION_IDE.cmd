@echo off
setlocal
cd /d "%~dp0"
if exist "KR_BUILD\Release\axion_ide.exe" (
    start "" "KR_BUILD\Release\axion_ide.exe"
    exit /b 0
)
if exist "KR_BUILD\Release\math_research_ide.exe" (
    start "" "KR_BUILD\Release\math_research_ide.exe"
    exit /b 0
)
echo Axion IDE nicht gefunden. Bitte zuerst das Projekt bauen:
echo   Start-Process -FilePath "H:\KR_COMPILER\KR-Portable-CPP-MultiHost_v2.4\KR-COMPILE.exe" -ArgumentList "--build", "H:\Axion", "--target", "WINDOWS", "--configuration", "Release" -Wait
pause
exit /b 1
