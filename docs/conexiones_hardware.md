# Conexiones de hardware

Fuente: `avances_ESP32.txt` (escaneo del 2026-10-20) y pinouts de fábrica. **Nada de esto se ha verificado con las
placas conectadas durante este trabajo.** Los pines están centralizados en `firmware/*/config.h`.

## Emisor — LILYGO TTGO T3 V1.6.1 (COM3, ESP32-PICO-D4, CH9102)

| Función | GPIO | Notas |
|---|---|---|
| LoRa SCK / MISO / MOSI / CS | 5 / 19 / 27 / 18 | SX1276, 915 MHz |
| LoRa RST / DIO0 | 23 / 26 | DIO1=33, DIO2=32 sin usar |
| AM2301 DATA | 14 | VCC→3V3, GND→GND. GPIO14 es SCK de la microSD: no usar la SD. Si da NaN con cable largo, pull-up 4.7–10 kΩ DATA↔3V3 |
| LED indicador | 25 | LED integrado |
| Salida «bombillo» | 4 | **Asunción**: GPIO4 libre; conecta un relé/MOSFET (no cargas de red directamente). Cambiable en `PIN_BOMBILLO` |
| Serie | 9600 baud | velocidad observada del firmware original |

Evitar: 21/22 (OLED), 34–39 (solo entrada), 6–11 (flash), 0/2/12/15 (arranque), 13 (SD).

## Receptor — ESP32-D0WDQ6 + CP2102 (COM4), **placa sin confirmar**

Heltec WiFi LoRa 32 V2 o TTGO LoRa32 V1.0 (mismos pines LoRa). Confirma la serigrafía y ajusta `PERFIL_PLACA` en `config.h`.

| Función | GPIO |
|---|---|
| LoRa SCK / MISO / MOSI / CS | 5 / 19 / 27 / 18 |
| LoRa RST / DIO0 | 14 / 26 |
| LED | 25 |
| Serie | 115200 baud |

## Antes de conectar

La guía exige validar las conexiones con el profesor antes de conectar la tarjeta al USB. Conecta las antenas LoRa
antes de transmitir. Hay un tercer emisor ajeno (`G3_EMISOR`) en el aire: sus paquetes se descartan por formato/checksum.

## Compilar y cargar

Emisor: `esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new`. Receptor: `esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M`
(cambia la tabla de particiones; el NVS queda en la misma dirección 0x9000). **No hagas volcados de flash públicos**: contienen credenciales Wi-Fi antiguas.
