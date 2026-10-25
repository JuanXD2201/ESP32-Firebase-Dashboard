# Compila los dos firmwares con Arduino CLI (https://arduino.github.io/arduino-cli/).
# Requiere arduino-cli en el PATH. Instala core y librerías con las versiones probadas.
$ErrorActionPreference = "Stop"
$raiz = Split-Path -Parent $PSScriptRoot

arduino-cli config init 2>$null
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.12
arduino-cli lib install "LoRa@0.8.0" "DHT sensor library@1.4.7" "Adafruit Unified Sensor@1.1.15"

# Emisor: LILYGO TTGO T3 V1.6.1
arduino-cli compile --fqbn "esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new" "$raiz\firmware\emisor"
if ($LASTEXITCODE) { exit 1 }

# Receptor: ESP32-D0WDQ6 de 4 MB. Esquema min_spiffs (1.9 MB de app) porque BLE + Wi-Fi + TLS no caben en 1.3 MB.
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M" "$raiz\firmware\receptor"
if ($LASTEXITCODE) { exit 1 }
Write-Host "Ambos firmwares compilaron."
