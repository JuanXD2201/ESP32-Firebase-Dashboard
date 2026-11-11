# Instala TODO lo necesario en un PC con Windows y deja el sistema funcionando:
#   Python + venv + dependencias de la API, arduino-cli + core esp32 2.0.12 + librerias,
#   .env de la API, compila y sube el firmware (emisor y receptor) y configura el Wi-Fi del receptor.
# Uso: doble clic en scripts\instalar_todo.bat   (o: powershell -ExecutionPolicy Bypass -File scripts\instalar_todo.ps1)
# Opciones: -Emisor COM8 -Receptor COM7   -SoloInstalar (sin placas)   -SinWifi (no configura el Wi-Fi)
param(
  [string]$Emisor = "",
  [string]$Receptor = "",
  [switch]$SoloInstalar,
  [switch]$SinWifi
)
$ErrorActionPreference = "Stop"
$raiz = Split-Path -Parent $PSScriptRoot
function Paso($t) { Write-Host "`n== $t ==" -ForegroundColor Cyan }
function Falla($t) { Write-Host "ERROR: $t" -ForegroundColor Red; exit 1 }
function RefrescarPath {
  $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
}

# ---------------------------------------------------------------- 1. Python
Paso "Python 3.10+"
function BuscarPython {
  foreach ($c in @("python", "py")) {
    $cmd = Get-Command $c -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source -notlike "*WindowsApps*") {
      $v = & $cmd.Source -c "import sys; print(sys.version_info[0]*100+sys.version_info[1])" 2>$null
      if ($v -and [int]$v -ge 310) { return $cmd.Source }
    }
  }
  return $null
}
$python = BuscarPython
if (-not $python) {
  if (-not (Get-Command winget -ErrorAction SilentlyContinue)) { Falla "Instala Python 3.12 desde python.org (marca 'Add to PATH') y vuelve a ejecutar." }
  Write-Host "Instalando Python 3.12 con winget..."
  winget install -e --id Python.Python.3.12 --accept-package-agreements --accept-source-agreements --silent
  RefrescarPath
  $python = BuscarPython
  if (-not $python) { Falla "Python se instalo pero no se ve en esta ventana. Cierra y vuelve a ejecutar el script." }
}
Write-Host "Python: $python"

# ---------------------------------------------------------------- 2. arduino-cli
Paso "arduino-cli"
$cli = (Get-Command arduino-cli -ErrorAction SilentlyContinue).Source
if (-not $cli) {
  $dir = Join-Path $env:LOCALAPPDATA "arduino-cli-bin"
  $cli = Join-Path $dir "arduino-cli.exe"
  if (-not (Test-Path $cli)) {
    Write-Host "Descargando arduino-cli..."
    New-Item -ItemType Directory -Force $dir | Out-Null
    $zip = Join-Path $env:TEMP "arduino-cli.zip"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip" -OutFile $zip -UseBasicParsing
    Expand-Archive $zip -DestinationPath $dir -Force
    Remove-Item $zip -Force
  }
}
Write-Host "arduino-cli: $cli"
& $cli config init 2>$null | Out-Null
& $cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json | Out-Null
& $cli core update-index
& $cli core install esp32:esp32@2.0.12
if ($LASTEXITCODE) { Falla "No se pudo instalar el core esp32 2.0.12." }
& $cli lib install "LoRa@0.8.0" "DHT sensor library@1.4.7" "Adafruit Unified Sensor@1.1.15"
if ($LASTEXITCODE) { Falla "No se pudieron instalar las librerias." }

# ---------------------------------------------------------------- 3. API (Python)
Paso "API: entorno virtual y dependencias"
$venvPy = Join-Path $raiz "backend\.venv\Scripts\python.exe"
if (-not (Test-Path $venvPy)) { & $python -m venv (Join-Path $raiz "backend\.venv"); if ($LASTEXITCODE) { Falla "No se pudo crear el entorno virtual." } }
& $venvPy -m pip install --quiet --upgrade pip
& $venvPy -m pip install --quiet -r (Join-Path $raiz "backend\requirements.txt")
if ($LASTEXITCODE) { Falla "Fallo pip install de la API." }

# ---------------------------------------------------------------- 4. backend\.env
Paso "Configuracion de la API (backend\.env)"
$envFile = Join-Path $raiz "backend\.env"
if (Test-Path $envFile) {
  Write-Host "backend\.env ya existe: se conserva."
} else {
  $proyecto = "esp32-980d5"
  $rc = Join-Path $raiz "firebase\.firebaserc"
  if (Test-Path $rc) { try { $proyecto = (Get-Content $rc -Raw | ConvertFrom-Json).projects.default } catch {} }
  $apiKey = & $venvPy -c "import secrets; print(secrets.token_urlsafe(24))"
  Write-Host "Para guardar en Firestore hace falta la clave JSON de la cuenta de servicio de Firebase (proyecto $proyecto)."
  Write-Host "No esta en el repositorio. Pasala desde otro PC o descargala en la consola de Firebase."
  $clave = (Read-Host "Ruta del archivo JSON (Enter = sin Firestore, solo memoria para pruebas)").Trim('"').Trim()
  if ($clave -and (Test-Path $clave)) { $storage = "firestore" } else { $storage = "memoria"; $clave = ""; Write-Host "Modo memoria: los datos NO llegan a Firestore ni al dashboard." -ForegroundColor Yellow }
  @(
    "STORAGE=$storage", "FIREBASE_PROJECT_ID=$proyecto", "GOOGLE_APPLICATION_CREDENTIALS=$clave",
    "FIRESTORE_COLLECTION=lecturas", "API_KEY=$apiKey", "CORS_ORIGINS=*", "MAX_LIMIT=500", "PORT=5000"
  ) | Set-Content -Path $envFile -Encoding ASCII
  Write-Host "Creado backend\.env (no se sube a git)."
}

if ($SoloInstalar) { Write-Host "`nInstalacion lista (sin placas)." -ForegroundColor Green; exit 0 }

# ---------------------------------------------------------------- 5. Placas
Paso "Placas ESP32"
$puertos = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
Write-Host ("Puertos detectados: " + ($puertos -join ", "))
if (-not $Emisor)   { $Emisor   = (Read-Host "Puerto del EMISOR (con el sensor AM2302), Enter = omitir").Trim().ToUpper() }
if (-not $Receptor) { $Receptor = (Read-Host "Puerto del RECEPTOR, Enter = omitir").Trim().ToUpper() }

$esptool = Get-ChildItem (Join-Path $env:LOCALAPPDATA "Arduino15\packages\esp32\tools\esptool_py") -Recurse -Filter "esptool.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
function TipoPlaca($puerto) {   # "t3" si es ESP32-PICO-D4 (LILYGO T3 V1.6.1), si no "heltec" (ESP32-D0WDQ6)
  if ($esptool) {
    $txt = (& $esptool.FullName --port $puerto chip_id 2>&1 | Out-String)
    if ($txt -match "PICO-D4") { return "t3" }
    if ($txt -match "Chip is") { return "heltec" }
  }
  $r = (Read-Host "No pude detectar la placa en $puerto. 1 = Heltec V2 / TTGO LoRa32 V1, 2 = LILYGO T3 V1.6.1").Trim()
  if ($r -eq "2") { return "t3" } else { return "heltec" }
}

function Subir($nombre, $sketch, $puerto, $fqbn, $perfil) {
  Write-Host "Compilando y subiendo $nombre a $puerto (tarda unos minutos)..."
  & $cli compile --fqbn $fqbn --build-property "compiler.cpp.extra_flags=-DPERFIL_PLACA=$perfil" (Join-Path $raiz "firmware\$sketch")
  if ($LASTEXITCODE) { Falla "No compilo $nombre." }
  & $cli upload -p $puerto --fqbn $fqbn (Join-Path $raiz "firmware\$sketch")
  if ($LASTEXITCODE) { Falla "No se pudo subir $nombre a $puerto (cierra Arduino IDE / monitores serie y revisa el cable)." }
  Write-Host "$nombre subido." -ForegroundColor Green
}

if ($Emisor) {
  $t = TipoPlaca $Emisor
  Write-Host "Emisor ($Emisor): placa $t"
  if ($t -eq "t3") { Subir "emisor" "emisor" $Emisor "esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new" 1 }   # emisor: T3=1
  else             { Subir "emisor" "emisor" $Emisor "esp32:esp32:esp32" 2 }                                      # emisor: Heltec=2
}
if ($Receptor) {
  $t = TipoPlaca $Receptor
  Write-Host "Receptor ($Receptor): placa $t"
  $fq = "esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M"
  if ($t -eq "t3") { Subir "receptor" "receptor" $Receptor $fq 3 }   # receptor: T3=3
  else             { Subir "receptor" "receptor" $Receptor $fq 1 }   # receptor: Heltec=1
}

# ---------------------------------------------------------------- 6. Wi-Fi + API
if ($Receptor -and -not $SinWifi) {
  Paso "Wi-Fi del receptor y API"
  Write-Host "El PC debe estar en la MISMA red Wi-Fi que usara el receptor."
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "conectar_receptor.ps1") -Puerto $Receptor
}

Write-Host "`nListo. Dashboard: https://esp32-980d5.web.app" -ForegroundColor Green
Write-Host "Para volver a lanzar la API: backend\.venv\Scripts\python -m app   (en la carpeta backend)"
