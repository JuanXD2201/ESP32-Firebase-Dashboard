// thingspeak.h - convierte un mensaje BLE del teléfono en el JSON de ThingSpeak.
// Sin Arduino: se prueba en el PC (tests/firmware/).
//
// Sintaxis del mensaje BLE:
//   "23.5"        -> field1 = 23.5
//   "3:18.2"      -> field3 = 18.2          (campo 1..8)
//   "hola mundo"  -> status = "hola mundo"  (texto libre; ThingSpeak lo guarda como estado)
#pragma once

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Escapa texto para una cadena JSON. Devuelve false si no cabe.
inline bool tsEscapar(char* destino, size_t capacidad, const char* origen) {
  size_t j = 0;
  for (const char* p = origen; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\') {
      if (j + 2 >= capacidad) return false;
      destino[j++] = '\\';
      destino[j++] = (char)c;
    } else if (c < 0x20 || c == 0x7F) {
      if (j + 1 >= capacidad) return false;
      destino[j++] = ' ';  // los controles se sustituyen por espacio
    } else {
      if (j + 1 >= capacidad) return false;
      destino[j++] = (char)c;
    }
  }
  destino[j] = 0;
  return true;
}

// Número decimal válido también como número JSON: -?dígitos(.dígitos)? (sin "nan", "inf", "+", ".5").
inline bool tsEsNumero(const char* s) {
  if (*s == '-') s++;
  if (!isdigit((unsigned char)*s)) return false;
  while (isdigit((unsigned char)*s)) s++;
  if (*s == '.') {
    s++;
    if (!isdigit((unsigned char)*s)) return false;
    while (isdigit((unsigned char)*s)) s++;
  }
  return *s == 0;
}

// `clave` es la Write API Key de ThingSpeak (solo caracteres alfanuméricos).
inline size_t tsConstruirJson(char* salida, size_t capacidad, const char* clave, const char* mensaje) {
  while (*mensaje == ' ') mensaje++;
  char texto[128];
  strncpy(texto, mensaje, sizeof(texto) - 1);
  texto[sizeof(texto) - 1] = 0;
  for (char* f = texto + strlen(texto); f > texto && isspace((unsigned char)f[-1]);) *--f = 0;
  if (texto[0] == 0) return 0;
  for (const char* c = clave; *c; c++) {
    if (!isalnum((unsigned char)*c)) return 0;  // evita inyección en el JSON
  }

  int campo = 0;
  const char* valor = texto;
  if (texto[0] >= '1' && texto[0] <= '8' && texto[1] == ':') {
    campo = texto[0] - '0';
    valor = texto + 2;
    if (!tsEsNumero(valor)) return 0;
  } else if (tsEsNumero(texto)) {
    campo = 1;
  }

  int n;
  if (campo > 0) {
    n = snprintf(salida, capacidad, "{\"api_key\":\"%s\",\"field%d\":%s}", clave, campo, valor);
  } else {
    char esc[2 * sizeof(texto)];
    if (!tsEscapar(esc, sizeof(esc), texto)) return 0;
    n = snprintf(salida, capacidad, "{\"api_key\":\"%s\",\"status\":\"%s\"}", clave, esc);
  }
  return (n > 0 && (size_t)n < capacidad) ? (size_t)n : 0;
}
