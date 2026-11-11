// config.h - configuración centralizada del EMISOR (LILYGO TTGO T3 V1.6.1, ESP32-PICO-D4).
// Fuente de los pines: escaneo de la placa (avances_ESP32.txt) y pinout de fábrica.
// El cableado del sensor y de la salida del bombillo NO está verificado físicamente aquí.
#pragma once

#include <Arduino.h>

// ---------- Identidad ----------
#define NODO_ID "emisor-01"            // [A-Za-z0-9_.-], máx 24 (ver protocolo.h)
#define BLE_NOMBRE "ESP32-Emisor-01"

// ---------- Serie ----------
#define SERIAL_BAUDIOS 9600            // el firmware original del emisor usaba 9600

// ---------- Perfil de placa (pines que cambian según el hardware) ----------
#define PERFIL_TTGO_T3_V16 1           // LILYGO T3 V1.6.1 (ESP32-PICO-D4)
#define PERFIL_HELTEC_V2 2             // Heltec WiFi LoRa 32 V2 / TTGO LoRa32 V1 (ESP32-D0WDQ6, 26 MHz)
#ifndef PERFIL_PLACA                    // se puede forzar al compilar: -DPERFIL_PLACA=<n>
#define PERFIL_PLACA PERFIL_HELTEC_V2
#endif

#if PERFIL_PLACA == PERFIL_TTGO_T3_V16
#define PIN_LORA_RST 23
#define PIN_AM2301 14                  // GPIO14 = SCK de la microSD: válido si no se usa la SD
#define PIN_BOMBILLO 4                 // salida al bombillo/relé (GPIO4 libre en esta placa)
#define PIN_LED 25
#elif PERFIL_PLACA == PERFIL_HELTEC_V2
#define PIN_LORA_RST 14                // en esta placa el RST del SX1276 es GPIO14
#define PIN_AM2301 13                  // AM2302 conectado aquí (conector DHT22 de la placa; detectado en ALTO con el sensor alimentado)
#define PIN_BOMBILLO 17                // suposición: libre, sin verificar; 4/15/16 son del OLED
#define PIN_LED 25                     // LED integrado (Heltec V2; en TTGO LoRa32 V1 puede ser otro pin)
#else
#error "PERFIL_PLACA desconocido"
#endif

// ---------- LoRa SX1276 ----------
#define PIN_LORA_SCK 5
#define PIN_LORA_MISO 19
#define PIN_LORA_MOSI 27
#define PIN_LORA_CS 18
#define PIN_LORA_DIO0 26
#define LORA_FRECUENCIA 915E6          // banda 915 MHz (observada en ambos firmwares)
#define LORA_SPREADING_FACTOR 7
#define LORA_ANCHO_BANDA 125E3
#define LORA_CODING_RATE 5             // 4/5
#define LORA_POTENCIA_DBM 17
#define LORA_SYNC_WORD 0x12

// ---------- Sensor AM2302 (ASAIR, = DHT22), 1 hilo ----------
#define TIPO_SENSOR DHT22
#define SENSOR_PERIODO_MS 5000         // intervalo entre lecturas/envíos
#define SENSOR_PERIODO_MIN_MS 2000     // límite del sensor (>= 2 s)
static_assert(SENSOR_PERIODO_MS >= SENSOR_PERIODO_MIN_MS, "El AM2301 exige >= 2 s entre lecturas");
#define SENSOR_FALLOS_MAX 5            // fallos seguidos antes de avisar con severidad

// ---------- Salidas ----------
#define BOMBILLO_ACTIVO_ALTO true      // false si el relé se activa con nivel bajo

// ---------- Regla automática del bombillo (histéresis) ----------
#define BOMBILLO_UMBRAL_ON_C 30.0f     // se enciende si temperatura >= umbral ON
#define BOMBILLO_UMBRAL_OFF_C 28.0f    // se apaga si temperatura <= umbral OFF

// ---------- Simulación explícita ----------
// 0 = datos reales del sensor. 1 = arranca en simulación. También: comando serie "sim on".
// Los paquetes simulados llevan simulado=1 y el dashboard los marca como tales.
#define SIMULACION_AL_ARRANCAR 0

// ---------- FreeRTOS ----------
#define COLA_TX_LONGITUD 8
#define COLA_LED_LONGITUD 8
#define TAREA_STACK_BYTES 4096

// ---------- BLE (comandos desde el teléfono) ----------
// Servicio estilo Nordic UART: compatible con nRF Connect / Serial Bluetooth Terminal (BLE).
#define BLE_SERVICIO_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"   // el teléfono escribe aquí
#define BLE_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"   // notificaciones al teléfono

// ---------- Comprobación de conflictos de GPIO ----------
#define _PIN_DISTINTOS(a, b) ((a) != (b))
static_assert(_PIN_DISTINTOS(PIN_AM2301, PIN_BOMBILLO) && _PIN_DISTINTOS(PIN_AM2301, PIN_LED) &&
                  _PIN_DISTINTOS(PIN_BOMBILLO, PIN_LED),
              "Conflicto de GPIO entre sensor, bombillo y LED");
static_assert(PIN_BOMBILLO != PIN_LORA_SCK && PIN_BOMBILLO != PIN_LORA_MISO &&
                  PIN_BOMBILLO != PIN_LORA_MOSI && PIN_BOMBILLO != PIN_LORA_CS &&
                  PIN_BOMBILLO != PIN_LORA_RST && PIN_BOMBILLO != PIN_LORA_DIO0,
              "El bombillo choca con un pin LoRa");
static_assert(PIN_AM2301 != PIN_LORA_SCK && PIN_AM2301 != PIN_LORA_MISO &&
                  PIN_AM2301 != PIN_LORA_MOSI && PIN_AM2301 != PIN_LORA_CS &&
                  PIN_AM2301 != PIN_LORA_RST && PIN_AM2301 != PIN_LORA_DIO0,
              "El sensor choca con un pin LoRa");
