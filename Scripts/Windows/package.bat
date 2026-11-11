@echo off
rem Iron Man Arena - make a stand-alone Windows game and a desktop shortcut.
rem Place this file in D:\IronManGame\ (next to the IronManArena folder) and run it after build.bat works.
rem It runs RunUAT BuildCookRun (Win64 Development: build, cook, stage, pak, archive) into D:\IronManGame\WindowsApp,
rem then points the desktop shortcut "Iron Man Game.lnk" at the packaged game.
setlocal EnableExtensions
set "HERE=%~dp0"
set "PROJ=%HERE%IronManArena\IronManArena.uproject"
if not exist "%PROJ%" set "PROJ=%HERE%IronManArena.uproject"
if not exist "%PROJ%" (
  echo [package] Cannot find IronManArena.uproject next to this script or in "%HERE%IronManArena\".
  goto :fail
)
set "OUT=%HERE%WindowsApp"
set "LOG=%HERE%package-win.log"

rem Find the engine the same way build.bat does.
if defined UE_ROOT if exist "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" goto :found
set "UE_ROOT="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE_ROOT=%%B"
if defined UE_ROOT if exist "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" goto :found
for %%D in ("C:\Program Files\Epic Games\UE_5.8" "D:\Program Files\Epic Games\UE_5.8" "D:\Epic Games\UE_5.8" "E:\Epic Games\UE_5.8" "D:\UE_5.8" "C:\UE_5.8") do (
  if exist "%%~D\Engine\Build\BatchFiles\RunUAT.bat" set "UE_ROOT=%%~D"
)
if defined UE_ROOT goto :found
echo [package] Unreal Engine 5.8 not found. Set it first, e.g.:
echo           set UE_ROOT=C:\Program Files\Epic Games\UE_5.8
goto :fail

:found
echo [package] Engine : %UE_ROOT%
echo [package] Project: %PROJ%
echo [package] Output : %OUT%
echo [package] This takes a while the first time (it cooks every asset). The full log goes to %LOG%
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJ%" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="%OUT%" -utf8output > "%LOG%" 2>&1
if errorlevel 1 (
  echo [package] Packaging failed. The last lines of the log:
  powershell -NoProfile -Command "Get-Content -Tail 40 '%LOG%'"
  goto :fail
)

set "EXE=%OUT%\Windows\IronManArena.exe"
if not exist "%EXE%" (
  echo [package] Packaging finished but %EXE% is missing. Look in %OUT% for the game folder.
  goto :fail
)

rem Point the desktop shortcut at the packaged game (works with OneDrive desktops too).
powershell -NoProfile -Command "$d=[Environment]::GetFolderPath('Desktop'); $s=(New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $d 'Iron Man Game.lnk')); $s.TargetPath='%EXE%'; $s.Arguments='-windowed -ResX=1600 -ResY=900'; $s.WorkingDirectory='%OUT%\Windows'; $s.IconLocation='%EXE%,0'; $s.Save()"
if errorlevel 1 (
  echo [package] The game is packaged, but the desktop shortcut could not be made.
  echo           Start it with: "%EXE%" -windowed -ResX=1600 -ResY=900
  goto :done
)
echo.
echo [package] Done. Double-click "Iron Man Game" on your desktop to play.

:done
endlocal
exit /b 0

:fail
pause
endlocal
exit /b 1
