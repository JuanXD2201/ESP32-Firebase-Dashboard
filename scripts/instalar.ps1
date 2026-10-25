# Instala dependencias del backend y del frontend (Windows / PowerShell 5+).
# Uso: .\scripts\instalar.ps1
$ErrorActionPreference = "Stop"
$raiz = Split-Path -Parent $PSScriptRoot

Write-Host "== Backend (Python) =="
Push-Location "$raiz\backend"
python -m venv .venv
& .\.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
if (-not (Test-Path .env)) { Copy-Item "$raiz\.env.example" .env; Write-Host "Creado backend\.env (edítalo)" }
Pop-Location

Write-Host "== Frontend (Node 20.19+ / 22.12+) =="
Push-Location "$raiz\frontend"
npm ci
Pop-Location

Write-Host "Listo. Pruebas: .\scripts\probar.ps1"
