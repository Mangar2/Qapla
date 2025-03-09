@echo off
setlocal

:: MinGW64 Pfad setzen (nur für dieses Skript)
set PATH=C:\WinLibs\mingw64\bin;%PATH%

:: Make ausführen
mingw32-make -j4

:: Falls Fehler auftreten, warten, bevor die Konsole schließt
if %ERRORLEVEL% neq 0 (
    echo Build failed.
    pause
)
