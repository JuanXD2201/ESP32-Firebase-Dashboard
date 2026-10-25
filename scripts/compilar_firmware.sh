#!/usr/bin/env bash
# Compila los dos firmwares con Arduino CLI (requiere arduino-cli en el PATH).
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"

arduino-cli config init 2>/dev/null || true
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.12
arduino-cli lib install "LoRa@0.8.0" "DHT sensor library@1.4.7" "Adafruit Unified Sensor@1.1.15"

arduino-cli compile --fqbn "esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new" "$RAIZ/firmware/emisor"
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M" "$RAIZ/firmware/receptor"
echo "Ambos firmwares compilaron."
