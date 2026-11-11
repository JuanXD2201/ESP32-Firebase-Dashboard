# Configura el receptor por serie (Wi-Fi + API + clave) y comprueba la conexión.
# Uso (con el PC ya en la misma red Wi-Fi que usará el receptor):
#   powershell -ExecutionPolicy Bypass -File scripts\conectar_receptor.ps1 [-Puerto COM7]
# La clave del Wi-Fi se pide por consola (no se muestra ni se guarda en el repo).
param([string]$Puerto = "COM7")

$raiz = Split-Path -Parent $PSScriptRoot
$ip = (Get-NetIPAddress -AddressFamily IPv4 |
  Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.*' -and $_.PrefixOrigin -ne 'WellKnown' } |
  Select-Object -First 1).IPAddress
if (-not $ip) { Write-Error "El PC no tiene IP de red."; exit 1 }
$api = "http://${ip}:5000"

$key = ((Get-Content "$raiz\backend\.env" | Where-Object { $_ -match '^API_KEY=' }) -replace '^API_KEY=', '').Trim().Trim('"')
if (-not $key) { Write-Error "Falta API_KEY en backend\.env"; exit 1 }

# 1) API local encendida
try { $h = Invoke-RestMethod "$api/health" -TimeoutSec 3 } catch { $h = $null }
if (-not $h) {
  Write-Host "Arrancando la API local..."
  Start-Process -WindowStyle Hidden -FilePath "$raiz\backend\.venv\Scripts\python.exe" -ArgumentList "-m", "app" -WorkingDirectory "$raiz\backend"
  for ($i = 0; $i -lt 30 -and -not $h; $i++) {
    Start-Sleep -Seconds 2
    try { $h = Invoke-RestMethod "$api/health" -TimeoutSec 3 } catch {}
  }
}
if ($h) { Write-Host "API: $($h.estado) ($($h.almacenamiento)) en $api" } else { Write-Error "La API no respondió."; exit 1 }

# 2) Datos del Wi-Fi
$ssid = Read-Host "SSID del Wi-Fi"
$clave = Read-Host "Clave del Wi-Fi" -AsSecureString
$claveTxt = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($clave))

# 3) Configurar el receptor
$s = New-Object System.IO.Ports.SerialPort $Puerto, 115200
$s.DtrEnable = $false; $s.RtsEnable = $false; $s.ReadTimeout = 500; $s.Open()
foreach ($c in @("api $api", "apikey $key", "wifi", $ssid, $claveTxt)) { $s.WriteLine($c); Start-Sleep -Milliseconds 800 }
Write-Host "Conectando al Wi-Fi (hasta 20 s)..."
Start-Sleep -Seconds 20
$s.WriteLine("estado")
$fin = [datetime]::Now.AddSeconds(4)
while ([datetime]::Now -lt $fin) {
  try { $l = $s.ReadLine().Trim(); if ($l -match 'ESTADO|WIFI|API|NUBE' -and $l -notmatch 'descartado') { Write-Host $l } } catch {}
}
$s.Close()
Write-Host "Si dice wifi=conectado, las lecturas del emisor suben solas. Dashboard: https://esp32-8aa66.web.app"
