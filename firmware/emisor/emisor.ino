// emisor.ino - NODO EMISOR LoRa + FreeRTOS (LILYGO TTGO T3 V1.6.1)
//
// Sensor AM2301 -> tarea Sensor -> cola -> tarea LoRa -> paquete 915 MHz -> receptor.
// BLE (teléfono) y serie permiten forzar el bombillo: "bombillo on|off|auto".
//
// Tareas FreeRTOS:
//   tareaSensor  lee el AM2301 (o simula si se pide), valida y aplica la regla del bombillo
//   tareaLoRa    serializa y transmite; lleva la secuencia y las estadísticas de radio
//   tareaLed     único dueño del LED integrado (patrones recibidos por cola)
//   tareaSerie   comandos por el monitor serie (ayuda, estado, bombillo, sim)
// BLE corre en la tarea propia de la pila Bluedroid y entrega comandos a procesarComando().
//
// Librerías: LoRa (Sandeep Mistry) 0.8.0, DHT sensor library 1.4.7, Adafruit Unified Sensor.
// Core: esp32 by Espressif 2.0.12. Ver firmware/emisor/README.md.

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <DHT.h>
#include <LoRa.h>
#include <Preferences.h>
#include <SPI.h>
#include <stdarg.h>

#include "config.h"
#include "protocolo.h"

// ---------------------------------------------------------------- estado compartido
enum LedPatron : uint8_t { LED_BLE_RECIBIDO };  // 3 ciclos de 250 ms encendido / 250 ms apagado

enum ModoBombillo : uint8_t { BOMBILLO_AUTO, BOMBILLO_FORZADO_ON, BOMBILLO_FORZADO_OFF };

struct Medicion {
  float temperatura;
  float humedad;
  bool bombillo;
  bool simulado;
};

struct Estadisticas {  // cada contador lo incrementa una sola tarea
  volatile uint32_t lecturasOk = 0;
  volatile uint32_t sensorFallos = 0;
  volatile uint32_t sensorFuera = 0;
  volatile uint32_t loraTx = 0;
  volatile uint32_t loraTxError = 0;
  volatile uint32_t colaLlena = 0;
  volatile uint32_t bleComandos = 0;
};

static Estadisticas stats;
static Preferences prefs;
static DHT sensorDht(PIN_AM2301, TIPO_SENSOR);
static BLECharacteristic* bleTx = nullptr;

static QueueHandle_t colaTx;
static QueueHandle_t colaLed;
static SemaphoreHandle_t mtxSerie;
static SemaphoreHandle_t mtxBombillo;

static uint32_t sesion = 0;
static uint32_t seq = 0;
static volatile bool simulacion = SIMULACION_AL_ARRANCAR;
static volatile bool bombilloEncendido = false;
static volatile ModoBombillo modoBombillo = BOMBILLO_AUTO;
static volatile bool loraListo = false;

// ---------------------------------------------------------------- registro serial
static void registrar(const char* formato, ...) {
  char buf[200];
  va_list args;
  va_start(args, formato);
  vsnprintf(buf, sizeof(buf), formato, args);
  va_end(args);
  if (xSemaphoreTake(mtxSerie, pdMS_TO_TICKS(200)) == pdTRUE) {
    Serial.println(buf);
    xSemaphoreGive(mtxSerie);
  }
}

// ---------------------------------------------------------------- LED
static void ledEvento(LedPatron p) {
  if (xQueueSend(colaLed, &p, 0) != pdTRUE) registrar("[LED] cola llena, patron descartado");
}

static void parpadear(int ciclos, int encendidoMs, int apagadoMs) {
  for (int i = 0; i < ciclos; i++) {
    digitalWrite(PIN_LED, HIGH);
    vTaskDelay(pdMS_TO_TICKS(encendidoMs));
    digitalWrite(PIN_LED, LOW);
    vTaskDelay(pdMS_TO_TICKS(apagadoMs));
  }
}

static void tareaLed(void*) {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  LedPatron patron;
  for (;;) {
    if (xQueueReceive(colaLed, &patron, portMAX_DELAY) == pdTRUE) {
      if (patron == LED_BLE_RECIBIDO) parpadear(3, 250, 250);
    }
  }
}

// ---------------------------------------------------------------- bombillo
static void aplicarSalidaBombillo(bool encendido) {
  bombilloEncendido = encendido;
  digitalWrite(PIN_BOMBILLO, (encendido == BOMBILLO_ACTIVO_ALTO) ? HIGH : LOW);
}

// Devuelve el estado resultante. La regla automática usa histéresis.
static bool actualizarBombillo(float temperatura) {
  xSemaphoreTake(mtxBombillo, portMAX_DELAY);
  bool estado = bombilloEncendido;
  if (modoBombillo == BOMBILLO_FORZADO_ON) estado = true;
  else if (modoBombillo == BOMBILLO_FORZADO_OFF) estado = false;
  else if (temperatura >= BOMBILLO_UMBRAL_ON_C) estado = true;
  else if (temperatura <= BOMBILLO_UMBRAL_OFF_C) estado = false;
  if (estado != bombilloEncendido) {
    aplicarSalidaBombillo(estado);
    registrar("[BOMBILLO] %s", estado ? "ENCENDIDO" : "APAGADO");
  }
  xSemaphoreGive(mtxBombillo);
  return estado;
}

static void forzarBombillo(ModoBombillo modo) {
  xSemaphoreTake(mtxBombillo, portMAX_DELAY);
  modoBombillo = modo;
  if (modo == BOMBILLO_FORZADO_ON) aplicarSalidaBombillo(true);
  if (modo == BOMBILLO_FORZADO_OFF) aplicarSalidaBombillo(false);
  xSemaphoreGive(mtxBombillo);
}

// ---------------------------------------------------------------- sensor
static bool leerSensor(Medicion* m) {
  if (simulacion) {
    static uint32_t n = 0;
    n++;
    m->temperatura = 24.0f + 4.0f * sinf(n / 10.0f);
    m->humedad = 55.0f + 10.0f * cosf(n / 13.0f);
    m->simulado = true;
    return true;
  }
  float t = sensorDht.readTemperature();
  float h = sensorDht.readHumidity();
  if (isnan(t) || isnan(h)) {
    stats.sensorFallos++;
    return false;
  }
  if (!protoRangoValido(t, h)) {
    stats.sensorFuera++;
    registrar("[SENSOR] lectura fuera de rango descartada: T=%.1f H=%.1f", t, h);
    return false;
  }
  m->temperatura = t;
  m->humedad = h;
  m->simulado = false;
  return true;
}

static void tareaSensor(void*) {
  sensorDht.begin();
  vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIODO_MIN_MS));  // el AM2301 necesita ~2 s tras energizar
  uint32_t fallosSeguidos = 0;
  TickType_t ultimo = xTaskGetTickCount();
  for (;;) {
    Medicion m = {};
    if (leerSensor(&m)) {
      fallosSeguidos = 0;
      stats.lecturasOk++;
      m.bombillo = actualizarBombillo(m.temperatura);
      registrar("[SENSOR] T=%.1f C  H=%.1f %%  bombillo=%s%s", m.temperatura, m.humedad,
                m.bombillo ? "ON" : "OFF", m.simulado ? "  (SIMULADO)" : "");
      if (xQueueSend(colaTx, &m, 0) != pdTRUE) {
        stats.colaLlena++;
        registrar("[SENSOR] cola de transmision llena: lectura descartada");
      }
    } else {
      fallosSeguidos++;
      registrar("[SENSOR] lectura fallida (%lu seguidas). Revisar cableado GPIO%d / pull-up",
                (unsigned long)fallosSeguidos, PIN_AM2301);
      if (fallosSeguidos == SENSOR_FALLOS_MAX) {
        registrar("[SENSOR] ERROR: %d fallos seguidos. No se enviaran datos inventados.",
                  SENSOR_FALLOS_MAX);
      }
    }
    vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(SENSOR_PERIODO_MS));
  }
}

// ---------------------------------------------------------------- LoRa
static bool iniciarLoRa() {
  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_CS);
  LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);
  if (!LoRa.begin(LORA_FRECUENCIA)) return false;
  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_ANCHO_BANDA);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setTxPower(LORA_POTENCIA_DBM);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.enableCrc();
  return true;
}

static void tareaLoRa(void*) {
  Medicion m;
  for (;;) {
    if (xQueueReceive(colaTx, &m, portMAX_DELAY) != pdTRUE) continue;
    if (!loraListo) {
      stats.loraTxError++;
      registrar("[LORA] radio no disponible: lectura no transmitida");
      continue;
    }
    LecturaLoRa l = {};
    strncpy(l.nodo, NODO_ID, PROTO_NODO_MAX);
    l.sesion = sesion;
    l.seq = seq;
    l.temperatura = m.temperatura;
    l.humedad = m.humedad;
    l.bombillo = m.bombillo;
    l.simulado = m.simulado;

    char trama[PROTO_TRAMA_MAX];
    size_t n = protoCodificar(trama, sizeof(trama), l);
    if (n == 0) {
      stats.loraTxError++;
      registrar("[LORA] no se pudo serializar la lectura");
      continue;
    }
    LoRa.beginPacket();
    LoRa.write((const uint8_t*)trama, n);
    bool ok = LoRa.endPacket() == 1;  // bloquea hasta terminar la transmisión
    if (ok) {
      stats.loraTx++;
      registrar("[LORA] TX #%lu OK: %s", (unsigned long)seq, trama);
    } else {
      stats.loraTxError++;
      registrar("[LORA] TX #%lu FALLO", (unsigned long)seq);
    }
    seq++;  // la secuencia avanza también si falla: el receptor verá el hueco
  }
}

// ---------------------------------------------------------------- comandos
static void imprimirEstado() {
  const char* modo = modoBombillo == BOMBILLO_AUTO
                         ? "auto"
                         : (modoBombillo == BOMBILLO_FORZADO_ON ? "forzado ON" : "forzado OFF");
  registrar("[ESTADO] nodo=%s sesion=%lu seq=%lu simulacion=%s lora=%s bombillo=%s (%s)", NODO_ID,
            (unsigned long)sesion, (unsigned long)seq, simulacion ? "SI" : "no",
            loraListo ? "ok" : "ERROR", bombilloEncendido ? "ON" : "OFF", modo);
  registrar("[ESTADO] lecturas=%lu fallosSensor=%lu fueraRango=%lu tx=%lu txError=%lu "
            "colaLlena=%lu bleCmd=%lu",
            (unsigned long)stats.lecturasOk, (unsigned long)stats.sensorFallos,
            (unsigned long)stats.sensorFuera, (unsigned long)stats.loraTx,
            (unsigned long)stats.loraTxError, (unsigned long)stats.colaLlena,
            (unsigned long)stats.bleComandos);
}

// Procesa un comando de texto (serie o BLE). Devuelve true si se reconoció.
static bool procesarComando(const char* entrada) {
  char cmd[48];
  strncpy(cmd, entrada, sizeof(cmd) - 1);
  cmd[sizeof(cmd) - 1] = 0;
  for (char* p = cmd; *p; p++) *p = (char)tolower((unsigned char)*p);
  char* fin = cmd + strlen(cmd);
  while (fin > cmd && (fin[-1] == ' ' || fin[-1] == '\r' || fin[-1] == '\n')) *--fin = 0;
  char* ini = cmd;
  while (*ini == ' ') ini++;

  if (!strcmp(ini, "bombillo on")) forzarBombillo(BOMBILLO_FORZADO_ON);
  else if (!strcmp(ini, "bombillo off")) forzarBombillo(BOMBILLO_FORZADO_OFF);
  else if (!strcmp(ini, "bombillo auto")) forzarBombillo(BOMBILLO_AUTO);
  else if (!strcmp(ini, "sim on")) simulacion = true;
  else if (!strcmp(ini, "sim off")) simulacion = false;
  else if (!strcmp(ini, "estado")) imprimirEstado();
  else if (!strcmp(ini, "ayuda")) {
    registrar("Comandos: estado | bombillo on|off|auto | sim on|off | ayuda");
  } else {
    return false;
  }
  if (!strncmp(ini, "sim ", 4)) {
    registrar("[SIM] simulacion %s%s", simulacion ? "ACTIVADA" : "desactivada",
              simulacion ? ": los paquetes llevan simulado=1" : "");
  }
  return true;
}

static void tareaSerie(void*) {
  char linea[64];
  size_t n = 0;
  uint32_t proximoEstado = millis() + 60000;
  for (;;) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n' || c == '\r') {
        if (n > 0) {
          linea[n] = 0;
          if (!procesarComando(linea)) registrar("[SERIE] comando desconocido (escribe: ayuda)");
          n = 0;
        }
      } else if (n < sizeof(linea) - 1) {
        linea[n++] = c;
      }
    }
    if ((int32_t)(millis() - proximoEstado) >= 0) {
      imprimirEstado();
      proximoEstado = millis() + 60000;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------------------------------------------------------- BLE
class CallbackServidor : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { registrar("[BLE] telefono conectado"); }
  void onDisconnect(BLEServer*) override {
    registrar("[BLE] telefono desconectado; publicitando de nuevo");
    BLEDevice::startAdvertising();
  }
};

class CallbackEscritura : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    auto valor = c->getValue();
    if (valor.length() == 0) return;
    char texto[48];
    size_t n = valor.length() < sizeof(texto) - 1 ? valor.length() : sizeof(texto) - 1;
    memcpy(texto, valor.c_str(), n);
    texto[n] = 0;
    stats.bleComandos++;
    registrar("[BLE] recibido: \"%s\"", texto);
    ledEvento(LED_BLE_RECIBIDO);
    bool ok = procesarComando(texto);
    const char* respuesta = ok ? "OK" : "ERROR: usa bombillo on|off|auto, sim on|off, estado";
    if (bleTx) {
      bleTx->setValue((uint8_t*)respuesta, strlen(respuesta));
      bleTx->notify();
    }
  }
};

static void iniciarBle() {
  BLEDevice::init(BLE_NOMBRE);
  BLEServer* servidor = BLEDevice::createServer();
  servidor->setCallbacks(new CallbackServidor());
  BLEService* servicio = servidor->createService(BLE_SERVICIO_UUID);
  bleTx = servicio->createCharacteristic(BLE_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  bleTx->addDescriptor(new BLE2902());
  BLECharacteristic* rx = servicio->createCharacteristic(
      BLE_RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new CallbackEscritura());
  servicio->start();
  BLEAdvertising* anuncio = BLEDevice::getAdvertising();
  anuncio->addServiceUUID(BLE_SERVICIO_UUID);
  anuncio->setScanResponse(true);
  BLEDevice::startAdvertising();
  registrar("[BLE] anunciando como \"%s\"", BLE_NOMBRE);
}

// ---------------------------------------------------------------- arranque
void setup() {
  Serial.begin(SERIAL_BAUDIOS);
  delay(300);
  mtxSerie = xSemaphoreCreateMutex();
  mtxBombillo = xSemaphoreCreateMutex();
  colaTx = xQueueCreate(COLA_TX_LONGITUD, sizeof(Medicion));
  colaLed = xQueueCreate(COLA_LED_LONGITUD, sizeof(LedPatron));

  registrar("");
  registrar("NODO EMISOR LoRa + FreeRTOS (LILYGO TTGO T3 V1.6.1) - %s", NODO_ID);

  pinMode(PIN_BOMBILLO, OUTPUT);
  aplicarSalidaBombillo(false);

  prefs.begin("emisor", false);
  sesion = prefs.getUInt("sesion", 0) + 1;  // identifica este arranque (deduplicacion en la API)
  prefs.putUInt("sesion", sesion);
  prefs.end();

  loraListo = iniciarLoRa();
  if (loraListo) registrar("[LORA] SX1276 listo a %.0f MHz, SF%d", LORA_FRECUENCIA / 1E6,
                           LORA_SPREADING_FACTOR);
  else registrar("[LORA] ERROR: sin respuesta SPI del SX1276. Revisar pines/antena.");

  iniciarBle();
  if (simulacion) registrar("[SIM] ARRANQUE EN MODO SIMULACION: los datos NO son mediciones");

  xTaskCreatePinnedToCore(tareaLed, "led", TAREA_STACK_BYTES, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(tareaSensor, "sensor", TAREA_STACK_BYTES, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(tareaLoRa, "lora", TAREA_STACK_BYTES, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(tareaSerie, "serie", TAREA_STACK_BYTES, nullptr, 1, nullptr, 1);
  registrar("Escribe 'ayuda' para ver los comandos.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));  // todo el trabajo ocurre en las tareas
}
