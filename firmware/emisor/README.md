# Firmware emisor (LILYGO TTGO T3 V1.6.1)

Lee el AM2302 (DHT22) cada 5 s (mínimo permitido 2 s), valida, aplica la regla del bombillo (ON ≥30 °C, OFF ≤28 °C, o forzado) y transmite por LoRa 915 MHz.

- Pines/constantes: `config.h`. Protocolo: `protocolo.h` (idéntico al del receptor).
- Librerías: LoRa 0.8.0 · DHT sensor library 1.4.7 · Adafruit Unified Sensor 1.1.15. Core esp32 2.0.12.
- FQBN: `esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new`.
- Serie 9600: `ayuda`, `estado`, `bombillo on|off|auto`, `sim on|off`.
- BLE «ESP32-Emisor-01» (servicio Nordic UART): escribe los mismos comandos en la característica `6E400002-…`; el LED hace 3×(250/250 ms).
- Un fallo del sensor **no** genera paquete. En simulación (`sim on`) los paquetes llevan `simulado=1`.
- Sin probar en hardware.

## Abrir en Arduino IDE
1. Abre `firmware/emisor/emisor.ino` (la carpeta se llama igual que el `.ino`; `config.h` y `protocolo.h` se cargan solos).
2. Gestor de tarjetas: instala **esp32 by Espressif Systems 2.0.12**.
3. Gestor de librerías: **LoRa** (Sandeep Mistry) 0.8.0, **DHT sensor library** (Adafruit) 1.4.7, **Adafruit Unified Sensor** 1.1.15.
4. Placa según el hardware (y `PERFIL_PLACA` en `config.h`):
   - `PERFIL_HELTEC_V2` (ESP32-D0WDQ6, 26 MHz, CP2102): **ESP32 Dev Module**. Sensor AM2302 en **GPIO13**, RST LoRa GPIO14.
   - `PERFIL_TTGO_T3_V16` (ESP32-PICO-D4): **TTGO LoRa32-OLED** con *Board Revision* `TTGO LoRa32 V2.1 (1.6.1)`. Sensor en GPIO14.
5. Cableado AM2302 (perfil Heltec): `+` → 3V3, `-` → GND, `out` → GPIO13.
6. Monitor serie a 9600 baudios.
