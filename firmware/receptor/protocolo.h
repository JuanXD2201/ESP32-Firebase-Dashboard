// protocolo.h - formato compacto de paquete LoRa (emisor -> receptor).
// Sin dependencias de Arduino: se prueba en el PC (tests/firmware/).
// Esta copia debe ser idéntica en firmware/emisor y firmware/receptor
// (la prueba tests/test_protocolo_sincronizado.py lo verifica).
//
// Trama ASCII:  nodo,sesion,seq,temp,hum,bombillo,simulado*CC
//   nodo      [A-Za-z0-9_.-]{1,24}
//   sesion    contador de arranques del emisor (uint32)
//   seq       número de secuencia del mensaje (uint32)
//   temp      °C con 1 decimal (-40.0 a 80.0)
//   hum       % con 1 decimal (0.0 a 100.0)
//   bombillo  0 | 1
//   simulado  0 | 1   (1 = valores generados, NO medidos)
//   CC        XOR de todos los bytes anteriores a '*', 2 dígitos hex mayúsculas
// Ejemplo:    emisor-01,3,42,23.4,58.2,1,0*1D
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROTO_NODO_MAX 24
#define PROTO_TRAMA_MAX 96

struct LecturaLoRa {
  char nodo[PROTO_NODO_MAX + 1];
  uint32_t sesion;
  uint32_t seq;
  float temperatura;
  float humedad;
  bool bombillo;
  bool simulado;
};

enum ProtoResultado {
  PROTO_OK = 0,
  PROTO_ERR_VACIO,
  PROTO_ERR_CRC,
  PROTO_ERR_FORMATO,
  PROTO_ERR_RANGO
};

inline const char* protoTexto(ProtoResultado r) {
  switch (r) {
    case PROTO_OK: return "ok";
    case PROTO_ERR_VACIO: return "vacio";
    case PROTO_ERR_CRC: return "checksum";
    case PROTO_ERR_FORMATO: return "formato";
    case PROTO_ERR_RANGO: return "rango";
  }
  return "?";
}

inline uint8_t protoChecksum(const char* datos, size_t n) {
  uint8_t x = 0;
  for (size_t i = 0; i < n; i++) x ^= (uint8_t)datos[i];
  return x;
}

inline bool protoNodoValido(const char* nodo) {
  size_t n = strlen(nodo);
  if (n == 0 || n > PROTO_NODO_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = nodo[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    if (!ok) return false;
  }
  return true;
}

inline bool protoRangoValido(float t, float h) {
  return !isnan(t) && !isnan(h) && t >= -40.0f && t <= 80.0f && h >= 0.0f && h <= 100.0f;
}

// Escribe la trama en `salida`. Devuelve su longitud o 0 si no cabe / datos inválidos.
inline size_t protoCodificar(char* salida, size_t capacidad, const LecturaLoRa& l) {
  if (!protoNodoValido(l.nodo) || !protoRangoValido(l.temperatura, l.humedad)) return 0;
  int n = snprintf(salida, capacidad, "%s,%lu,%lu,%.1f,%.1f,%d,%d", l.nodo,
                   (unsigned long)l.sesion, (unsigned long)l.seq, (double)l.temperatura,
                   (double)l.humedad, l.bombillo ? 1 : 0, l.simulado ? 1 : 0);
  if (n <= 0 || (size_t)n + 4 > capacidad) return 0;
  uint8_t cc = protoChecksum(salida, (size_t)n);
  int m = snprintf(salida + n, capacidad - n, "*%02X", cc);
  return (m == 3) ? (size_t)(n + m) : 0;
}

// Lee un campo numérico sin signo terminado en ',' (o fin). Avanza `*p`.
inline bool protoLeerUint(const char** p, uint32_t* destino, char terminador) {
  char* fin = nullptr;
  if (**p < '0' || **p > '9') return false;
  unsigned long v = strtoul(*p, &fin, 10);
  if (fin == *p || *fin != terminador) return false;
  *destino = (uint32_t)v;
  *p = fin + 1;
  return true;
}

inline bool protoLeerFloat(const char** p, float* destino) {
  char* fin = nullptr;
  float v = strtof(*p, &fin);
  if (fin == *p || *fin != ',') return false;
  *destino = v;
  *p = fin + 1;
  return true;
}

inline bool protoLeerBool(const char** p, bool* destino, char terminador) {
  if ((**p != '0' && **p != '1') || (*p)[1] != terminador) return false;
  *destino = (**p == '1');
  *p += 2;
  return true;
}

// Decodifica y valida una trama. `salida` solo es válida si devuelve PROTO_OK.
inline ProtoResultado protoDecodificar(const char* trama, size_t largo, LecturaLoRa* salida) {
  if (trama == nullptr || largo == 0) return PROTO_ERR_VACIO;
  if (largo >= PROTO_TRAMA_MAX) return PROTO_ERR_FORMATO;

  // Localizar '*CC' al final (exactamente 3 caracteres).
  if (largo < 4 || trama[largo - 3] != '*') return PROTO_ERR_FORMATO;
  char* finHex = nullptr;
  char hex[3] = {trama[largo - 2], trama[largo - 1], 0};
  for (int i = 0; i < 2; i++) {
    char c = hex[i];
    bool esHex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    if (!esHex) return PROTO_ERR_FORMATO;
  }
  unsigned long recibido = strtoul(hex, &finHex, 16);
  size_t cuerpo = largo - 3;
  if (protoChecksum(trama, cuerpo) != (uint8_t)recibido) return PROTO_ERR_CRC;

  char copia[PROTO_TRAMA_MAX];
  memcpy(copia, trama, cuerpo);
  copia[cuerpo] = ',';  // terminador uniforme para el último campo
  copia[cuerpo + 1] = 0;

  LecturaLoRa l;
  memset(&l, 0, sizeof(l));
  const char* p = copia;
  const char* coma = strchr(p, ',');
  if (coma == nullptr) return PROTO_ERR_FORMATO;
  size_t n = (size_t)(coma - p);
  if (n == 0 || n > PROTO_NODO_MAX) return PROTO_ERR_FORMATO;
  memcpy(l.nodo, p, n);
  l.nodo[n] = 0;
  if (!protoNodoValido(l.nodo)) return PROTO_ERR_FORMATO;
  p = coma + 1;

  if (!protoLeerUint(&p, &l.sesion, ',')) return PROTO_ERR_FORMATO;
  if (!protoLeerUint(&p, &l.seq, ',')) return PROTO_ERR_FORMATO;
  if (!protoLeerFloat(&p, &l.temperatura)) return PROTO_ERR_FORMATO;
  if (!protoLeerFloat(&p, &l.humedad)) return PROTO_ERR_FORMATO;
  if (!protoLeerBool(&p, &l.bombillo, ',')) return PROTO_ERR_FORMATO;
  if (!protoLeerBool(&p, &l.simulado, ',')) return PROTO_ERR_FORMATO;
  if (*p != 0) return PROTO_ERR_FORMATO;  // campos de más

  if (!protoRangoValido(l.temperatura, l.humedad)) return PROTO_ERR_RANGO;
  *salida = l;
  return PROTO_OK;
}

// JSON lógico para la API. rssi/snr son mediciones del receptor; se omiten si no hay.
inline size_t protoAJson(char* salida, size_t capacidad, const LecturaLoRa& l, bool hayRadio,
                         int rssi, float snr) {
  int n = snprintf(salida, capacidad,
                   "{\"nodo\":\"%s\",\"sesion\":%lu,\"seq\":%lu,\"temperatura\":%.1f,"
                   "\"humedad\":%.1f,\"bombillo\":%s,\"simulado\":%s",
                   l.nodo, (unsigned long)l.sesion, (unsigned long)l.seq, (double)l.temperatura,
                   (double)l.humedad, l.bombillo ? "true" : "false",
                   l.simulado ? "true" : "false");
  if (n <= 0 || (size_t)n >= capacidad) return 0;
  if (hayRadio) {
    int m = snprintf(salida + n, capacidad - n, ",\"rssi\":%d,\"snr\":%.1f", rssi, (double)snr);
    if (m <= 0 || (size_t)(n + m) >= capacidad) return 0;
    n += m;
  }
  if ((size_t)n + 2 > capacidad) return 0;
  salida[n++] = '}';
  salida[n] = 0;
  return (size_t)n;
}
