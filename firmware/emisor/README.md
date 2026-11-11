# Firmware emisor (LILYGO TTGO T3 V1.6.1)

Lee el AM2301 cada 5 s (mínimo permitido 2 s), valida, aplica la regla del bombillo (ON ≥30 °C, OFF ≤28 °C, o forzado) y transmite por LoRa 915 MHz.

- Pines/constantes: `config.h`. Protocolo: `protocolo.h` (idéntico al del receptor).
- Librerías: LoRa 0.8.0 · DHT sensor library 1.4.7 · Adafruit Unified Sensor 1.1.15. Core esp32 2.0.12.
- FQBN: `esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new`.
- Serie 9600: `ayuda`, `estado`, `bombillo on|off|auto`, `sim on|off`.
- BLE «ESP32-Emisor-01» (servicio Nordic UART): escribe los mismos comandos en la característica `6E400002-…`; el LED hace 3×(250/250 ms).
- Un fallo del sensor **no** genera paquete. En simulación (`sim on`) los paquetes llevan `simulado=1`.
- Sin probar en hardware.
