// Pruebas en el PC de la lógica pura del firmware (sin hardware):
//   protocolo.h (trama LoRa, checksum, JSON), rastreo.h (duplicados/pérdidas), mensajes.h (BLE → JSON).
// Ejecutar: python tests/firmware/run_firmware_tests.py
#include <cstdio>
#include <cstring>
#include <string>

#include "../../firmware/receptor/protocolo.h"
#include "../../firmware/receptor/rastreo.h"
#include "../../firmware/receptor/mensajes.h"

static int fallos = 0;
static int total = 0;
#define VERIFICAR(cond)                                                       \
  do {                                                                        \
    total++;                                                                  \
    if (!(cond)) {                                                            \
      fallos++;                                                               \
      std::printf("FALLO %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
    }                                                                         \
  } while (0)

static LecturaLoRa ejemplo() {
  LecturaLoRa l;
  memset(&l, 0, sizeof(l));
  strcpy(l.nodo, "emisor-01");
  l.sesion = 3;
  l.seq = 42;
  l.temperatura = 23.4f;
  l.humedad = 58.2f;
  l.bombillo = true;
  l.simulado = false;
  return l;
}

static void trama_ida_y_vuelta() {
  char t[PROTO_TRAMA_MAX];
  size_t n = protoCodificar(t, sizeof(t), ejemplo());
  VERIFICAR(n > 0);
  VERIFICAR(strncmp(t, "emisor-01,3,42,23.4,58.2,1,0*", 29) == 0);
  LecturaLoRa r;
  VERIFICAR(protoDecodificar(t, n, &r) == PROTO_OK);
  VERIFICAR(strcmp(r.nodo, "emisor-01") == 0 && r.sesion == 3 && r.seq == 42);
  VERIFICAR(r.temperatura > 23.39f && r.temperatura < 23.41f);
  VERIFICAR(r.humedad > 58.19f && r.humedad < 58.21f);
  VERIFICAR(r.bombillo && !r.simulado);
}

static void checksum_documentado() {
  // Si cambia el algoritmo, el emisor y el receptor dejarían de entenderse.
  const char* cuerpo = "emisor-01,3,42,23.4,58.2,1,0";
  char esperado[8];
  snprintf(esperado, sizeof(esperado), "*%02X", protoChecksum(cuerpo, strlen(cuerpo)));
  char t[PROTO_TRAMA_MAX];
  protoCodificar(t, sizeof(t), ejemplo());
  VERIFICAR(strcmp(t + strlen(cuerpo), esperado) == 0);
}

static void rechaza_paquetes_corruptos() {
  LecturaLoRa r;
  char t[PROTO_TRAMA_MAX];
  size_t n = protoCodificar(t, sizeof(t), ejemplo());

  char mala[PROTO_TRAMA_MAX];
  memcpy(mala, t, n + 1);
  mala[12] ^= 0x01;  // un bit alterado en el cuerpo
  VERIFICAR(protoDecodificar(mala, n, &r) == PROTO_ERR_CRC);

  VERIFICAR(protoDecodificar("", 0, &r) == PROTO_ERR_VACIO);
  VERIFICAR(protoDecodificar(nullptr, 5, &r) == PROTO_ERR_VACIO);
  VERIFICAR(protoDecodificar("G3_EMISOR,21.4,56.3,0", 21, &r) == PROTO_ERR_FORMATO);  // formato ajeno
  VERIFICAR(protoDecodificar("RECEPTOR:Mensaje de prueba USB #2", 33, &r) == PROTO_ERR_FORMATO);
  VERIFICAR(protoDecodificar("abc*ZZ", 6, &r) == PROTO_ERR_FORMATO);

  std::string largo(200, 'a');
  VERIFICAR(protoDecodificar(largo.c_str(), largo.size(), &r) == PROTO_ERR_FORMATO);
}

// Construye una trama válida (con checksum correcto) a partir de un cuerpo arbitrario.
static std::string con_checksum(const std::string& cuerpo) {
  char hex[8];
  snprintf(hex, sizeof(hex), "*%02X", protoChecksum(cuerpo.c_str(), cuerpo.size()));
  return cuerpo + hex;
}

static void valida_campos_y_rangos() {
  LecturaLoRa r;
  auto prueba = [&](const char* cuerpo) {
    std::string s = con_checksum(cuerpo);
    return protoDecodificar(s.c_str(), s.size(), &r);
  };
  VERIFICAR(prueba("emisor-01,1,0,25.0,50.0,0,0") == PROTO_OK);
  VERIFICAR(prueba("emisor-01,1,0,95.0,50.0,0,0") == PROTO_ERR_RANGO);   // temperatura imposible
  VERIFICAR(prueba("emisor-01,1,0,25.0,120.0,0,0") == PROTO_ERR_RANGO);  // humedad imposible
  VERIFICAR(prueba("emisor-01,1,0,nan,50.0,0,0") == PROTO_ERR_RANGO);
  VERIFICAR(prueba("emisor-01,1,0,25.0,50.0,2,0") == PROTO_ERR_FORMATO); // bool inválido
  VERIFICAR(prueba("emisor-01,1,0,25.0,50.0,1") == PROTO_ERR_FORMATO);   // faltan campos
  VERIFICAR(prueba("emisor-01,1,0,25.0,50.0,1,0,9") == PROTO_ERR_FORMATO);  // sobran campos
  VERIFICAR(prueba("emisor 01,1,0,25.0,50.0,1,0") == PROTO_ERR_FORMATO);  // nodo con espacio
  VERIFICAR(prueba(",1,0,25.0,50.0,1,0") == PROTO_ERR_FORMATO);           // nodo vacío
  VERIFICAR(prueba("emisor-01,-1,0,25.0,50.0,1,0") == PROTO_ERR_FORMATO); // sesion negativa
  VERIFICAR(prueba("emisor-01,1,0,25.0abc,50.0,1,0") == PROTO_ERR_FORMATO);
  VERIFICAR(prueba("emisor-01,1,0,-40.0,0.0,0,1") == PROTO_OK);            // límites inclusivos
  VERIFICAR(r.simulado);
}

static void codificar_rechaza_datos_invalidos() {
  char t[PROTO_TRAMA_MAX];
  LecturaLoRa l = ejemplo();
  l.temperatura = NAN;
  VERIFICAR(protoCodificar(t, sizeof(t), l) == 0);  // nunca se transmite un NaN
  l = ejemplo();
  l.humedad = 101.0f;
  VERIFICAR(protoCodificar(t, sizeof(t), l) == 0);
  l = ejemplo();
  strcpy(l.nodo, "mal nodo");
  VERIFICAR(protoCodificar(t, sizeof(t), l) == 0);
  l = ejemplo();
  VERIFICAR(protoCodificar(t, 10, l) == 0);  // buffer insuficiente
}

static void json_para_la_api() {
  char j[300];
  LecturaLoRa l = ejemplo();
  size_t n = protoAJson(j, sizeof(j), l, true, -65, 9.5f);
  VERIFICAR(n > 0);
  VERIFICAR(strcmp(j,
                   "{\"nodo\":\"emisor-01\",\"sesion\":3,\"seq\":42,\"temperatura\":23.4,"
                   "\"humedad\":58.2,\"bombillo\":true,\"simulado\":false,\"rssi\":-65,"
                   "\"snr\":9.5}") == 0);
  n = protoAJson(j, sizeof(j), l, false, 0, 0);  // sin radio: se omiten rssi y snr
  VERIFICAR(n > 0 && strstr(j, "rssi") == nullptr && j[n - 1] == '}');
  VERIFICAR(protoAJson(j, 20, l, true, -65, 9.5f) == 0);  // no cabe
}

static void rastreador_duplicados_y_perdidas() {
  Rastreador r(1000, 16);
  LecturaLoRa l = ejemplo();
  uint32_t perdidos = 99;

  l.seq = 0;
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_NUEVO && perdidos == 0);
  l.seq = 1;
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_NUEVO && perdidos == 0);
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_DUPLICADO);  // misma (nodo, sesion, seq)
  l.seq = 5;
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_NUEVO && perdidos == 3);  // faltaron 2,3,4
  l.seq = 3;
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_NUEVO && perdidos == 0);  // llegó tarde, no es duplicado
  l.seq = 3;
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_DUPLICADO);

  // El emisor reinicia: sesion nueva, seq vuelve a 0 y NO es duplicado ni pérdida.
  l.sesion = 4;
  l.seq = 0;
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_NUEVO && perdidos == 0);

  // Otro nodo con la misma seq no es duplicado.
  strcpy(l.nodo, "emisor-02");
  l.sesion = 1;
  l.seq = 0;
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_NUEVO);
}

static void rastreador_salto_enorme_no_cuenta_perdidas() {
  Rastreador r(1000, 16);
  LecturaLoRa l = ejemplo();
  uint32_t perdidos = 0;
  l.seq = 1;
  r.evaluar(l, &perdidos);
  l.seq = 5000000;
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_NUEVO && perdidos == 0);
}

static void rastreador_ventana_se_desplaza() {
  Rastreador r(1000, 4);
  LecturaLoRa l = ejemplo();
  uint32_t perdidos;
  for (uint32_t i = 0; i < 10; i++) {
    l.seq = i;
    VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_NUEVO);
  }
  l.seq = 9;
  VERIFICAR(r.evaluar(l, &perdidos) == RASTREO_DUPLICADO);
}

static void mensajes_ble_a_json() {
  char j[300];
  VERIFICAR(msgConstruirJson(j, sizeof(j), "receptor-01", "hola") > 0);
  VERIFICAR(strcmp(j, "{\"nodo\":\"receptor-01\",\"texto\":\"hola\"}") == 0);
  VERIFICAR(msgConstruirJson(j, sizeof(j), "receptor-01", "  hola mundo \r\n") > 0);
  VERIFICAR(strcmp(j, "{\"nodo\":\"receptor-01\",\"texto\":\"hola mundo\"}") == 0);
  VERIFICAR(msgConstruirJson(j, sizeof(j), "receptor-01", "di \"hola\"\\") > 0);
  VERIFICAR(strcmp(j, "{\"nodo\":\"receptor-01\",\"texto\":\"di \\\"hola\\\"\\\\\"}") == 0);
  VERIFICAR(msgConstruirJson(j, sizeof(j), "receptor-01", "a\nb") > 0 && strstr(j, "a b") != nullptr);  // control -> espacio
  VERIFICAR(msgConstruirJson(j, sizeof(j), "receptor-01", "   ") == 0);       // vacío
  VERIFICAR(msgConstruirJson(j, sizeof(j), "mal nodo", "x") == 0);            // nodo inválido
  VERIFICAR(msgConstruirJson(j, sizeof(j), "r\"x", "x") == 0);                // inyección por el nodo
  std::string largo(300, 'z');
  VERIFICAR(msgConstruirJson(j, sizeof(j), "receptor-01", largo.c_str()) > 0);
  VERIFICAR(strlen(j) == 31 + 100 + 2);                                                // truncado a 100 caracteres
  VERIFICAR(msgConstruirJson(j, 20, "receptor-01", "hola") == 0);             // no cabe
}

int main() {
  trama_ida_y_vuelta();
  checksum_documentado();
  rechaza_paquetes_corruptos();
  valida_campos_y_rangos();
  codificar_rechaza_datos_invalidos();
  json_para_la_api();
  rastreador_duplicados_y_perdidas();
  rastreador_salto_enorme_no_cuenta_perdidas();
  rastreador_ventana_se_desplaza();
  mensajes_ble_a_json();
  std::printf("%d verificaciones, %d fallos\n", total, fallos);
  return fallos == 0 ? 0 : 1;
}
