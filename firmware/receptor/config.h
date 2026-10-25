// config.h - configuración centralizada del RECEPTOR / pasarela LoRa -> Wi-Fi -> nube.
// La revisión exacta de la placa NO está confirmada (Heltec WiFi LoRa 32 V2 o TTGO LoRa32 V1.0,
// ver avances_ESP32.txt). Ambas comparten los pines LoRa reportados. Confirma la serigrafía y
// ajusta PERFIL_PLACA si difiere. No se ha probado en hardware.
#pragma once

#include <Arduino.h>

// ---------- Perfil de placa ----------
#define PERFIL_HELTEC_V2 1
#define PERFIL_TTGO_LORA32_V1 2
#define PERFIL_PLACA PERFIL_HELTEC_V2

#if PERFIL_PLACA == PERFIL_HELTEC_V2 || PERFIL_PLACA == PERFIL_TTGO_LORA32_V1
// SX1276 (pines reportados por el escaneo; coinciden en ambos perfiles)
#define PIN_LORA_SCK 5
#define PIN_LORA_MISO 19
#define PIN_LORA_MOSI 27
#define PIN_LORA_CS 18
#define PIN_LORA_RST 14
#define PIN_LORA_DIO0 26
#define PIN_LED 25
#else
#error "PERFIL_PLACA desconocido: define los pines en config.h"
#endif

// ---------- Serie ----------
#define SERIAL_BAUDIOS 115200

// ---------- LoRa (deben coincidir con el emisor) ----------
#define LORA_FRECUENCIA 915E6
#define LORA_SPREADING_FACTOR 7
#define LORA_ANCHO_BANDA 125E3
#define LORA_CODING_RATE 5
#define LORA_SYNC_WORD 0x12

// ---------- Recepción ----------
#define DEDUP_VENTANA 16               // últimas (nodo,sesion,seq) recordadas
#define PERDIDOS_SALTO_MAX 1000        // saltos de seq mayores se consideran reinicio, no pérdida

// ---------- BLE (mensajes desde el teléfono) ----------
#define BLE_NOMBRE "ESP32-Receptor"
#define BLE_SERVICIO_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_MENSAJE_MAX 100

// ---------- Wi-Fi y nube ----------
#define WIFI_CONEXION_TIMEOUT_MS 15000
#define WIFI_REINTENTO_MIN_MS 5000
#define WIFI_REINTENTO_MAX_MS 60000
#define HTTP_TIMEOUT_MS 5000
#define HTTP_REINTENTOS 3
#define HTTP_PAUSA_REINTENTO_MS 2000
#define API_URL_POR_DEFECTO ""              // URL base de la API; se configura por serie: api <url>
#define RECEPTOR_ID "receptor-01"           // nodo con el que se registran los mensajes BLE

// ---------- FreeRTOS ----------
#define COLA_NUBE_LONGITUD 20
#define COLA_LED_LONGITUD 8
#define TAREA_STACK_BYTES 4096
#define TAREA_RED_STACK_BYTES 8192

// ---------- Comprobación de conflictos de GPIO ----------
static_assert(PIN_LED != PIN_LORA_SCK && PIN_LED != PIN_LORA_MISO && PIN_LED != PIN_LORA_MOSI &&
                  PIN_LED != PIN_LORA_CS && PIN_LED != PIN_LORA_RST && PIN_LED != PIN_LORA_DIO0,
              "El LED choca con un pin LoRa");
