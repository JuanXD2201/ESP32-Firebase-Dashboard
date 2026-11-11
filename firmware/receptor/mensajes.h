// mensajes.h - convierte un mensaje BLE del teléfono en el JSON de POST /api/mensajes.
// Sin Arduino: se prueba en el PC (tests/firmware/).
//   {"nodo":"receptor-01","texto":"hola"}
#pragma once

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MSG_TEXTO_MAX 100

// Escapa texto para una cadena JSON. Los caracteres de control pasan a espacio. false si no cabe.
inline bool msgEscapar(char* destino, size_t capacidad, const char* origen) {
  size_t j = 0;
  for (const char* p = origen; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\') {
      if (j + 2 >= capacidad) return false;
      destino[j++] = '\\';
      destino[j++] = (char)c;
    } else if (c < 0x20 || c == 0x7F) {
      if (j + 1 >= capacidad) return false;
      destino[j++] = ' ';
    } else {
      if (j + 1 >= capacidad) return false;
      destino[j++] = (char)c;
    }
  }
  destino[j] = 0;
  return true;
}

// Devuelve la longitud del JSON, o 0 si el mensaje está vacío / el nodo es inválido / no cabe.
// `nodo` debe ser un identificador [A-Za-z0-9_.-] (se verifica).
inline size_t msgConstruirJson(char* salida, size_t capacidad, const char* nodo, const char* mensaje) {
  if (nodo == nullptr || nodo[0] == 0) return 0;
  for (const char* c = nodo; *c; c++) {
    bool ok = isalnum((unsigned char)*c) || *c == '_' || *c == '-' || *c == '.';
    if (!ok) return 0;
  }
  while (*mensaje == ' ' || *mensaje == '\t') mensaje++;
  char texto[MSG_TEXTO_MAX + 1];
  strncpy(texto, mensaje, MSG_TEXTO_MAX);
  texto[MSG_TEXTO_MAX] = 0;
  for (char* f = texto + strlen(texto); f > texto && isspace((unsigned char)f[-1]);) *--f = 0;
  if (texto[0] == 0) return 0;

  char esc[2 * MSG_TEXTO_MAX + 1];
  if (!msgEscapar(esc, sizeof(esc), texto)) return 0;
  int n = snprintf(salida, capacidad, "{\"nodo\":\"%s\",\"texto\":\"%s\"}", nodo, esc);
  return (n > 0 && (size_t)n < capacidad) ? (size_t)n : 0;
}
