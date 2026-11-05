@echo off
rem pwsh (PowerShell 7) s'il existe, sinon Windows PowerShell 5
where pwsh >nul 2>&1 && (set "PS=pwsh") || (set "PS=powershell")
%PS% -NoProfile -ExecutionPolicy Bypass -File "%~dp0Installer-tout.ps1" -Retirer
pause
