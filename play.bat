@echo off
set "SRC=%~dp0game_x64.dll"
set "DST=g:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease\baseq2\game_x64.dll"
set "EXE=g:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease\quake2ex_steam.exe"

if not exist "%SRC%" (
    echo ERROR: Source not found: %SRC%
    pause
    exit /b 1
)

copy /y "%SRC%" "%DST%"
if errorlevel 1 (
    echo ERROR: Copy failed. Check that the destination path exists and is not locked.
    pause
    exit /b 1
)

echo Copy successful. Launching game...
start "" "%EXE%"
