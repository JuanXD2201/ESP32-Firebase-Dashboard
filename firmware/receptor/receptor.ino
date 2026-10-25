// receptor.ino - RECEPTOR LoRa + BLE + Wi-Fi (pasarela a la nube) con FreeRTOS
//
//   LoRa 915 MHz --> tareaLoRa --(cola)--> tareaRed --HTTP POST--> API (-> Firestore)
//   Teléfono --BLE--> callback --(cola)--> tareaRed --HTTP POST--> API (-> Firestore) --> dashboard
//   Serie: tareaSerie (Wi-Fi y ajustes en cualquier momento)   LED: tareaLed (único dueño)
//
// Requisitos de la guía "Capa de red en IoT" cubiertos en este mismo código:
//   1 Wi-Fi por serie en cualquier momento    -> comando "wifi"
//   2 BLE mostrado por serie                  -> CallbackEscritura
//   3 LED 3x(250 ms ON/250 ms OFF) al recibir BLE
//   4 HTTP POST (JSON) a la nube              -> tareaRed (API /api/lecturas y /api/mensajes)
//   5 LED 2x(500 ms ON/500 ms OFF) al éxito de la nube
//   6 FreeRTOS: tareaLoRa (comunicaciones), tareaRed, tareaLed, tareaSerie
//
// Librerías: LoRa 0.8.0. Core esp32 2.0.12. Ver firmware/receptor/README.md.

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <LoRa.h>
#include <SPI.h>
#include <WiFi.h>
#include <stdarg.h>

#include "ajustes.h"
#include "config.h"
#include "nube.h"
#include "protocolo.h"
#include "rastreo.h"
#include "mensajes.h"

// ---------------------------------------------------------------- tipos y estado
enum LedPatron : uint8_t {
  LED_BLE_RECIBIDO,  // 3 ciclos de 250 ms encendido / 250 ms apagado
  LED_NUBE_OK        // 2 ciclos de 500 ms encendido / 500 ms apagado
};

enum TrabajoTipo : uint8_t { TRABAJO_LECTURA, TRABAJO_BLE };

// Declarado arriba: el preprocesador de Arduino genera prototipos antes del resto del código.
enum EstadoSerie : uint8_t { SERIE_NORMAL, SERIE_ESPERA_SSID, SERIE_ESPERA_CLAVE };

struct Trabajo {
  TrabajoTipo tipo;
  char datos[256];  // JSON de la lectura, o texto BLE
};

struct Estadisticas {  // cada contador lo escribe una sola tarea
  volatile uint32_t loraRecibidos = 0;     // tareaLoRa
  volatile uint32_t loraInvalidos = 0;
  volatile uint32_t loraDuplicados = 0;
  volatile uint32_t loraPerdidos = 0;
  volatile uint32_t bleMensajes = 0;       // callback BLE
  volatile uint32_t nubeOk = 0;            // tareaRed
  volatile uint32_t nubeError = 0;
  volatile uint32_t nubeSinConfig = 0;
  volatile uint32_t colaDescartados = 0;   // productores
  volatile uint32_t wifiCaidas = 0;        // tareaRed
};

static Estadisticas stats;
static Rastreador rastreador(PERDIDOS_SALTO_MAX, DEDUP_VENTANA);
static QueueHandle_t colaNube;
static QueueHandle_t colaLed;
static SemaphoreHandle_t mtxSerie;
static TaskHandle_t tareaRedHandle = nullptr;
static BLECharacteristic* bleTx = nullptr;
static volatile bool loraListo = false;
static volatile int ultimoRssi = 0;
static volatile float ultimoSnr = 0;

// ---------------------------------------------------------------- registro serial
static void registrar(const char* formato, ...) {
  char buf[240];
  va_list args;
  va_start(args, formato);
  vsnprintf(buf, sizeof(buf), formato, args);
  va_end(args);
  if (xSemaphoreTake(mtxSerie, pdMS_TO_TICKS(200)) == pdTRUE) {
    Serial.println(buf);
    xSemaphoreGive(mtxSerie);
  }
}

// ---------------------------------------------------------------- LED (único dueño: tareaLed)
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
    if (xQueueReceive(colaLed, &patron, portMAX_DELAY) != pdTRUE) continue;
    if (patron == LED_BLE_RECIBIDO) parpadear(3, 250, 250);
    else if (patron == LED_NUBE_OK) parpadear(2, 500, 500);
  }
}

// ---------------------------------------------------------------- cola hacia la nube
// Si la cola está llena se descarta el trabajo MÁS ANTIGUO (y se cuenta): lo más nuevo importa más.
static void encolarNube(const Trabajo& t) {
  if (xQueueSend(colaNube, &t, 0) == pdTRUE) return;
  Trabajo viejo;
  if (xQueueReceive(colaNube, &viejo, 0) == pdTRUE) {
    stats.colaDescartados++;
    registrar("[COLA] llena: se descarta el trabajo mas antiguo");
  }
  if (xQueueSend(colaNube, &t, 0) != pdTRUE) {
    stats.colaDescartados++;
    registrar("[COLA] llena: trabajo nuevo descartado");
  }
}

// ---------------------------------------------------------------- LoRa (comunicaciones)
static bool iniciarLoRa() {
  SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_CS);
  LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);
  if (!LoRa.begin(LORA_FRECUENCIA)) return false;
  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_ANCHO_BANDA);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.enableCrc();
  LoRa.receive();  // modo de recepción continua
  return true;
}

static void procesarPaquete(const char* trama, size_t largo, int rssi, float snr) {
  LecturaLoRa l;
  ProtoResultado r = protoDecodificar(trama, largo, &l);
  if (r != PROTO_OK) {
    stats.loraInvalidos++;
    registrar("[LORA] paquete descartado (%s) rssi=%d: \"%.40s\"", protoTexto(r), rssi, trama);
    return;
  }
  uint32_t perdidos = 0;
  if (rastreador.evaluar(l, &perdidos) == RASTREO_DUPLICADO) {
    stats.loraDuplicados++;
    registrar("[LORA] duplicado ignorado: %s ses=%lu seq=%lu", l.nodo, (unsigned long)l.sesion,
              (unsigned long)l.seq);
    return;
  }
  stats.loraRecibidos++;
  if (perdidos > 0) {
    stats.loraPerdidos += perdidos;
    registrar("[LORA] aviso: faltaron %lu paquete(s) antes de seq=%lu", (unsigned long)perdidos,
              (unsigned long)l.seq);
  }
  ultimoRssi = rssi;
  ultimoSnr = snr;
  registrar("[LORA] RX %s seq=%lu T=%.1f H=%.1f bombillo=%s rssi=%d snr=%.1f%s", l.nodo,
            (unsigned long)l.seq, l.temperatura, l.humedad, l.bombillo ? "ON" : "OFF", rssi, snr,
            l.simulado ? " (SIMULADO)" : "");

  Trabajo t;
  t.tipo = TRABAJO_LECTURA;
  if (protoAJson(t.datos, sizeof(t.datos), l, true, rssi, snr) == 0) {
    registrar("[LORA] no se pudo construir el JSON");
    return;
  }
  encolarNube(t);
}

static void tareaLoRa(void*) {
  char trama[PROTO_TRAMA_MAX];
  for (;;) {
    if (!loraListo) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    int tam = LoRa.parsePacket();
    if (tam > 0) {
      size_t n = 0;
      bool demasiado = tam >= (int)sizeof(trama);
      while (LoRa.available()) {
        int c = LoRa.read();
        if (n < sizeof(trama) - 1) trama[n++] = (char)c;
      }
      trama[n] = 0;
      if (demasiado) {
        stats.loraInvalidos++;
        registrar("[LORA] paquete de %d bytes descartado (demasiado largo)", tam);
      } else {
        procesarPaquete(trama, n, LoRa.packetRssi(), LoRa.packetSnr());
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
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
    size_t n = valor.length();
    if (n == 0) return;
    if (n > BLE_MENSAJE_MAX) n = BLE_MENSAJE_MAX;
    Trabajo t;
    t.tipo = TRABAJO_BLE;
    memcpy(t.datos, valor.c_str(), n);
    t.datos[n] = 0;
    for (char* p = t.datos + strlen(t.datos); p > t.datos && (p[-1] == '\n' || p[-1] == '\r');) *--p = 0;
    if (t.datos[0] == 0) return;

    stats.bleMensajes++;
    registrar("[BLE] mensaje recibido: \"%s\"", t.datos);  // requisito 2
    ledEvento(LED_BLE_RECIBIDO);                            // requisito 3
    encolarNube(t);                                         // requisito 4
    if (bleTx) {
      bleTx->setValue((uint8_t*)"OK", 2);
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

// ---------------------------------------------------------------- red (Wi-Fi + HTTP)
static bool wifiConectadoLog = false;
static bool wifiIntentando = false;
static uint32_t wifiInicioIntento = 0;
static uint32_t wifiProximoIntento = 0;
static uint32_t wifiPausaMs = WIFI_REINTENTO_MIN_MS;

// Máquina de estados sin esperas: nunca bloquea LoRa ni BLE (otras tareas).
static void gestionarWifi(const Ajustes& a) {
  if (a.ssid[0] == 0) return;
  uint32_t ahora = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConectadoLog) {
      wifiConectadoLog = true;
      wifiIntentando = false;
      wifiPausaMs = WIFI_REINTENTO_MIN_MS;
      registrar("[WIFI] conectado a \"%s\" IP=%s rssi=%d", a.ssid, WiFi.localIP().toString().c_str(),
                WiFi.RSSI());
    }
    return;
  }
  if (wifiConectadoLog) {
    wifiConectadoLog = false;
    stats.wifiCaidas++;
    registrar("[WIFI] conexion perdida; se reintentara");
    wifiProximoIntento = ahora;
  }
  if (wifiIntentando) {
    if (ahora - wifiInicioIntento > WIFI_CONEXION_TIMEOUT_MS) {
      WiFi.disconnect(true);
      wifiIntentando = false;
      wifiProximoIntento = ahora + wifiPausaMs;
      registrar("[WIFI] no se pudo conectar a \"%s\"; reintento en %lu s", a.ssid,
                (unsigned long)(wifiPausaMs / 1000));
      wifiPausaMs = min<uint32_t>(wifiPausaMs * 2, WIFI_REINTENTO_MAX_MS);
    }
    return;
  }
  if ((int32_t)(ahora - wifiProximoIntento) >= 0) {
    registrar("[WIFI] conectando a \"%s\"...", a.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(a.ssid, a.clave);
    wifiIntentando = true;
    wifiInicioIntento = ahora;
  }
}

static bool codigoExitoso(int codigo) { return codigo == 200 || codigo == 201; }

// URL base configurada (sin "/" final ni el sufijo "/api/lecturas" si el usuario lo pegó) + ruta.
static void construirUrl(char* salida, size_t n, const char* base, const char* ruta) {
  char b[160];
  strncpy(b, base, sizeof(b) - 1);
  b[sizeof(b) - 1] = 0;
  size_t len = strlen(b);
  while (len > 0 && b[len - 1] == '/') b[--len] = 0;
  const char* sufijo = "/api/lecturas";
  size_t ls = strlen(sufijo);
  if (len >= ls && strcmp(b + len - ls, sufijo) == 0) b[len - ls] = 0;
  snprintf(salida, n, "%s%s", b, ruta);
}

// Envía un trabajo con reintentos. Devuelve true solo si la nube confirmó la recepción.
static bool enviarTrabajo(const Trabajo& t, const Ajustes& a) {
  char cuerpo[300];
  char url[200];
  const char* destino = t.tipo == TRABAJO_LECTURA ? "API lecturas" : "API mensajes";

  if (a.apiUrl[0] == 0) {
    stats.nubeSinConfig++;
    registrar("[API] sin configurar: usa el comando  api http://<IP-del-PC>:5000");
    return false;
  }
  if (t.tipo == TRABAJO_LECTURA) {
    construirUrl(url, sizeof(url), a.apiUrl, "/api/lecturas");
    strncpy(cuerpo, t.datos, sizeof(cuerpo) - 1);
    cuerpo[sizeof(cuerpo) - 1] = 0;
  } else {
    construirUrl(url, sizeof(url), a.apiUrl, "/api/mensajes");
    if (msgConstruirJson(cuerpo, sizeof(cuerpo), RECEPTOR_ID, t.datos) == 0) {
      stats.nubeError++;
      registrar("[API] mensaje BLE vacio o no convertible a JSON");
      return false;
    }
  }
  const char* cabecera = "X-API-Key";
  const char* clave = a.apiKey;

  for (int intento = 1; intento <= HTTP_REINTENTOS; intento++) {
    if (WiFi.status() != WL_CONNECTED) {
      registrar("[%s] sin Wi-Fi: envio no realizado", destino);
      stats.nubeError++;
      return false;
    }
    String respuesta;
    registrar("[%s] POST (intento %d/%d): %.120s", destino, intento, HTTP_REINTENTOS, cuerpo);
    int codigo = nubePostJson(url, cabecera, clave, cuerpo, &respuesta);

    bool ok = codigoExitoso(codigo);
    if (ok) {
      stats.nubeOk++;
      registrar("[%s] OK HTTP %d  respuesta=%s", destino, codigo, respuesta.c_str());
      return true;
    }
    if (codigo > 0) registrar("[%s] FALLO HTTP %d  respuesta=%s", destino, codigo, respuesta.c_str());
    else registrar("[%s] FALLO de conexion: %s", destino, HTTPClient::errorToString(codigo).c_str());
    // 4xx de validación no se arregla reintentando.
    if (codigo >= 400 && codigo < 500 && codigo != 408 && codigo != 429) break;
    if (intento < HTTP_REINTENTOS) vTaskDelay(pdMS_TO_TICKS(HTTP_PAUSA_REINTENTO_MS));
  }
  stats.nubeError++;
  return false;
}

static void tareaRed(void*) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  Trabajo t;
  for (;;) {
    if (ulTaskNotifyTake(pdTRUE, 0) > 0) {  // credenciales nuevas desde la tarea serie
      WiFi.disconnect(true);
      wifiConectadoLog = false;
      wifiIntentando = false;
      wifiProximoIntento = 0;
      wifiPausaMs = WIFI_REINTENTO_MIN_MS;
    }
    Ajustes a = ajustesLeer();
    gestionarWifi(a);

    // Se deja el trabajo en la cola hasta que haya Wi-Fi: no se pierde por estar offline.
    if (WiFi.status() == WL_CONNECTED && xQueueReceive(colaNube, &t, pdMS_TO_TICKS(200)) == pdTRUE) {
      if (enviarTrabajo(t, a)) ledEvento(LED_NUBE_OK);  // requisito 5
    } else {
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }
}

// ---------------------------------------------------------------- serie (comandos y Wi-Fi)
static void imprimirEstado() {
  Ajustes a = ajustesLeer();
  registrar("[ESTADO] placa=%s lora=%s ultimo rssi=%d snr=%.1f", PERFIL_PLACA == PERFIL_HELTEC_V2 ? "Heltec V2 (sin confirmar)" : "TTGO LoRa32 V1 (sin confirmar)",
            loraListo ? "ok" : "ERROR", (int)ultimoRssi, (double)ultimoSnr);
  registrar("[ESTADO] wifi=%s ssid=\"%s\" ip=%s", WiFi.status() == WL_CONNECTED ? "conectado" : "desconectado",
            a.ssid[0] ? a.ssid : "(sin configurar)",
            WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-");
  registrar("[ESTADO] api=%s  apiKey=%s", a.apiUrl[0] ? a.apiUrl : "(sin configurar)",
            a.apiKey[0] ? "definida" : "no");
  registrar("[ESTADO] lora: rx=%lu invalidos=%lu duplicados=%lu perdidos=%lu | ble=%lu", (unsigned long)stats.loraRecibidos,
            (unsigned long)stats.loraInvalidos, (unsigned long)stats.loraDuplicados,
            (unsigned long)stats.loraPerdidos, (unsigned long)stats.bleMensajes);
  registrar("[ESTADO] nube: ok=%lu error=%lu sinConfig=%lu colaDescartados=%lu wifiCaidas=%lu",
            (unsigned long)stats.nubeOk, (unsigned long)stats.nubeError, (unsigned long)stats.nubeSinConfig,
            (unsigned long)stats.colaDescartados, (unsigned long)stats.wifiCaidas);
}

static void imprimirAyuda() {
  registrar("Comandos (en cualquier momento):");
  registrar("  wifi               pide SSID y luego la clave (la clave se ve al escribirla)");
  registrar("  wifi borrar        elimina las credenciales Wi-Fi guardadas");
  registrar("  api <url>          URL base de la API, ej. http://192.168.1.50:5000");
  registrar("  apikey <clave>     clave X-API-Key de la API (vacia: apikey -)");
  registrar("  estado | ayuda | reiniciar");
}

static void procesarLinea(char* linea, EstadoSerie* estado, char* ssidTemporal) {
  if (*estado == SERIE_ESPERA_SSID) {
    if (linea[0] == 0) {
      registrar("[WIFI] SSID vacio: cancelado");
      *estado = SERIE_NORMAL;
      return;
    }
    strncpy(ssidTemporal, linea, 32);
    ssidTemporal[32] = 0;
    registrar("[WIFI] SSID \"%s\" recibido. Escribe la clave y pulsa Enter:", ssidTemporal);
    *estado = SERIE_ESPERA_CLAVE;
    return;
  }
  if (*estado == SERIE_ESPERA_CLAVE) {
    ajustesGuardarWifi(ssidTemporal, linea);
    registrar("[WIFI] credenciales guardadas (%u caracteres de clave). Conectando...", (unsigned)strlen(linea));
    *estado = SERIE_NORMAL;
    if (tareaRedHandle) xTaskNotifyGive(tareaRedHandle);
    return;
  }

  if (!strcmp(linea, "wifi")) {
    registrar("[WIFI] Escribe el SSID y pulsa Enter:");
    *estado = SERIE_ESPERA_SSID;
  } else if (!strcmp(linea, "wifi borrar")) {
    ajustesGuardarWifi("", "");
    if (tareaRedHandle) xTaskNotifyGive(tareaRedHandle);
    registrar("[WIFI] credenciales eliminadas");
  } else if (!strncmp(linea, "api ", 4)) {
    const char* url = linea + 4;
    if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) {
      registrar("[API] la URL debe empezar por http:// o https://");
    } else {
      ajustesGuardarApiUrl(url);
      registrar("[API] URL guardada: %s", url);
    }
  } else if (!strncmp(linea, "apikey ", 7)) {
    ajustesGuardarApiKey(strcmp(linea + 7, "-") ? linea + 7 : "");
    registrar("[API] apiKey actualizada");
  } else if (!strcmp(linea, "estado")) {
    imprimirEstado();
  } else if (!strcmp(linea, "ayuda")) {
    imprimirAyuda();
  } else if (!strcmp(linea, "reiniciar")) {
    registrar("Reiniciando...");
    delay(200);
    ESP.restart();
  } else if (linea[0]) {
    registrar("[SERIE] comando desconocido (escribe: ayuda)");
  }
}

static void tareaSerie(void*) {
  char linea[128];
  char ssidTemporal[33] = {0};
  size_t n = 0;
  EstadoSerie estado = SERIE_NORMAL;
  uint32_t proximoEstado = millis() + 60000;
  for (;;) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n' || c == '\r') {
        linea[n] = 0;
        if (n > 0 || estado != SERIE_NORMAL) procesarLinea(linea, &estado, ssidTemporal);
        n = 0;
      } else if (n < sizeof(linea) - 1) {
        linea[n++] = c;
      }
    }
    if ((int32_t)(millis() - proximoEstado) >= 0) {
      if (estado == SERIE_NORMAL) imprimirEstado();
      proximoEstado = millis() + 60000;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------------------------------------------------------- arranque
void setup() {
  Serial.begin(SERIAL_BAUDIOS);
  delay(300);
  mtxSerie = xSemaphoreCreateMutex();
  colaNube = xQueueCreate(COLA_NUBE_LONGITUD, sizeof(Trabajo));
  colaLed = xQueueCreate(COLA_LED_LONGITUD, sizeof(LedPatron));
  ajustesIniciar();

  registrar("");
  registrar("RECEPTOR LoRa + BLE + Wi-Fi + FreeRTOS");

  loraListo = iniciarLoRa();
  if (loraListo) registrar("[LORA] SX1276 listo a %.0f MHz, SF%d", LORA_FRECUENCIA / 1E6, LORA_SPREADING_FACTOR);
  else registrar("[LORA] ERROR: sin respuesta SPI del SX1276. Revisar pines (config.h) y antena.");

  iniciarBle();

  Ajustes a = ajustesLeer();
  if (a.ssid[0] == 0) registrar("[WIFI] sin credenciales. Escribe  wifi  para configurarlas.");

  xTaskCreatePinnedToCore(tareaLed, "led", TAREA_STACK_BYTES, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(tareaLoRa, "lora", TAREA_STACK_BYTES, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(tareaSerie, "serie", TAREA_STACK_BYTES, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(tareaRed, "red", TAREA_RED_STACK_BYTES, nullptr, 2, &tareaRedHandle, 0);
  registrar("Escribe 'ayuda' para ver los comandos.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));  // todo el trabajo ocurre en las tareas
}
