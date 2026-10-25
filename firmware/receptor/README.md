# Firmware receptor / pasarela (ESP32-D0WDQ6, placa sin confirmar)

LoRa → valida → cola → HTTP POST a la API. BLE (teléfono) → serie + LED + API (`/api/mensajes` → Firestore → dashboard). Wi-Fi configurable por serie en cualquier momento.

- Pines: `config.h` (`PERFIL_PLACA`: Heltec V2 / TTGO LoRa32 V1, ambos con los pines reportados por el escaneo; confirma la serigrafía).
- Librerías: LoRa 0.8.0. Core esp32 2.0.12.
- FQBN: `esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M` (≈1.7 MB; no cabe en el esquema por defecto).
- Serie 115200. Comandos: `wifi` (pide SSID y clave), `wifi borrar`, `api <url base>` (ej. `http://192.168.1.50:5000`), `apikey <clave>`, `estado`, `ayuda`, `reiniciar`.
  Los ajustes se guardan en NVS (no en el código). La clave Wi-Fi se ve en el monitor mientras se escribe: no compartas capturas.
- BLE «ESP32-Receptor» (Nordic UART, escribir en `6E400002-…`). Cada escritura (hasta 100 caracteres) es un mensaje de texto: se imprime por serie, hace parpadear el LED y se envía como `{"nodo":"receptor-01","texto":"..."}`.
- LED: BLE ⇒ 3×(250/250 ms); éxito de nube ⇒ 2×(500/500 ms). Un solo dueño (`tareaLed`).
- La URL `https://` valida el certificado con el bundle del core (enlaza; sin probar en dispositivo). `http://` va sin cifrar: úsalo solo en red local.
- Sin probar en hardware.
