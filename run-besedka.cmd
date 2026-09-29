@echo off
rem Собирает Беседку в Debug и запускает её. Цель Besedka — приложение и то,
rem на чём оно держится, без тестов.
setlocal
set "ROOT=%~dp0"
set "APP=%ROOT%build\x64\App\Debug"

pwsh -NoLogo -NoProfile -File "%ROOT%tools\build.ps1" -Target Besedka
if errorlevel 1 (
    rem LNK1168 — Беседка ещё открыта и держит свой exe.
    pause
    exit /b 1
)

start "" /D "%APP%" "%APP%\Besedka.exe"
