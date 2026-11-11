@echo off
rem Doble clic: instala todo y deja el sistema funcionando (ver instalar_todo.ps1).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0instalar_todo.ps1" %*
pause
