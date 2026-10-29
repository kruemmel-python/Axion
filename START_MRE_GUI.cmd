@echo off
setlocal
cd /d "%~dp0"
set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
if not exist "%CMAKE%" (
 echo CMake nicht gefunden: %CMAKE%
 pause
 exit /b 1
)
"%CMAKE%" -S "mre-gui" -B "mre-gui\build-windows" -G "Visual Studio 17 2022" -A x64 || goto error
"%CMAKE%" --build "mre-gui\build-windows" --config Release --target MRE_Research_GUI || goto error
start "" "mre-gui\build-windows\Release\MRE_Research_GUI.exe"
exit /b 0
:error
echo GUI-Build fehlgeschlagen.
pause
exit /b 1
