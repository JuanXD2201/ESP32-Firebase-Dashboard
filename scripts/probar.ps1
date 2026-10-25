# Ejecuta todas las pruebas automatizadas disponibles (no requiere hardware ni Firebase).
$ErrorActionPreference = "Stop"
$raiz = Split-Path -Parent $PSScriptRoot
$fallos = 0

Push-Location "$raiz\backend"; & .\.venv\Scripts\python.exe -m pytest -q; if ($LASTEXITCODE) { $fallos++ }; Pop-Location
Push-Location "$raiz\frontend"; npm test; if ($LASTEXITCODE) { $fallos++ }; npm run build; if ($LASTEXITCODE) { $fallos++ }; Pop-Location
& "$raiz\backend\.venv\Scripts\python.exe" "$raiz\tests\firmware\run_firmware_tests.py"
if ($LASTEXITCODE -eq 2) { Write-Warning "Pruebas de firmware omitidas: falta compilador C++ (pip install ziglang)" }
elseif ($LASTEXITCODE) { $fallos++ }

if ($fallos) { Write-Error "$fallos grupo(s) de pruebas fallaron"; exit 1 }
Write-Host "Todas las pruebas ejecutadas pasaron."
