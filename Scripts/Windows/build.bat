@echo off
rem Iron Man Arena - Windows build helper.
rem Place this file in D:\IronManGame\ (next to the IronManArena folder) and run it.
rem It compiles the IronManArenaEditor (Win64 Development) target with UE 5.8 + Visual Studio 2022 (MSVC).
setlocal EnableExtensions
set "HERE=%~dp0"
set "PROJ=%HERE%IronManArena\IronManArena.uproject"
if not exist "%PROJ%" set "PROJ=%HERE%IronManArena.uproject"
if not exist "%PROJ%" (
  echo [build] Cannot find IronManArena.uproject next to this script or in "%HERE%IronManArena\".
  goto :fail
)

rem 1) UE_ROOT environment variable, 2) Epic launcher registry entry, 3) common install folders.
if defined UE_ROOT if exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" goto :found
set "UE_ROOT="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE_ROOT=%%B"
if defined UE_ROOT if exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" goto :found
for %%D in ("C:\Program Files\Epic Games\UE_5.8" "D:\Program Files\Epic Games\UE_5.8" "D:\Epic Games\UE_5.8" "E:\Epic Games\UE_5.8" "D:\UE_5.8" "C:\UE_5.8") do (
  if exist "%%~D\Engine\Build\BatchFiles\Build.bat" set "UE_ROOT=%%~D"
)
if defined UE_ROOT goto :found
echo [build] Unreal Engine 5.8 not found. Set it first, e.g.:
echo         set UE_ROOT=C:\Program Files\Epic Games\UE_5.8
goto :fail

:found
echo [build] Engine : %UE_ROOT%
echo [build] Project: %PROJ%
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" IronManArenaEditor Win64 Development -Project="%PROJ%" -WaitMutex
if errorlevel 1 goto :fail
echo.
echo [build] Succeeded. Launch with:
echo   "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" -game -windowed -ResX=1600 -ResY=900
echo   (or open the .uproject in the editor and press Play)
endlocal
exit /b 0

:fail
echo [build] FAILED - see the messages above (and %%LOCALAPPDATA%%\UnrealBuildTool\Log.txt).
pause
endlocal
exit /b 1
