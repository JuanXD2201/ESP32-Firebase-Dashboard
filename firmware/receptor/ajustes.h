// ajustes.h - ajustes persistentes en NVS (Wi-Fi, URL de la API, claves). Nunca se escriben en
// el código fuente ni se imprimen completos por serie.
#pragma once

#include <Preferences.h>

#include "config.h"

struct Ajustes {
  char ssid[33];
  char clave[65];
  char apiUrl[160];
  char apiKey[64];
};

static SemaphoreHandle_t mtxAjustes = nullptr;
static Ajustes ajustesActuales;

static void ajustesCopiar(char* destino, size_t n, const String& origen) {
  strncpy(destino, origen.c_str(), n - 1);
  destino[n - 1] = 0;
}

static void ajustesIniciar() {
  mtxAjustes = xSemaphoreCreateMutex();
  Preferences p;
  p.begin("receptor", true);
  memset(&ajustesActuales, 0, sizeof(ajustesActuales));
  ajustesCopiar(ajustesActuales.ssid, sizeof(ajustesActuales.ssid), p.getString("ssid", ""));
  ajustesCopiar(ajustesActuales.clave, sizeof(ajustesActuales.clave), p.getString("clave", ""));
  ajustesCopiar(ajustesActuales.apiUrl, sizeof(ajustesActuales.apiUrl),
                p.getString("apiUrl", API_URL_POR_DEFECTO));
  ajustesCopiar(ajustesActuales.apiKey, sizeof(ajustesActuales.apiKey), p.getString("apiKey", ""));
  p.end();
}

// Copia coherente de los ajustes para quien los lee desde otra tarea.
static Ajustes ajustesLeer() {
  Ajustes copia;
  xSemaphoreTake(mtxAjustes, portMAX_DELAY);
  copia = ajustesActuales;
  xSemaphoreGive(mtxAjustes);
  return copia;
}

static void ajustesGuardarTexto(const char* clave, char* destino, size_t n, const char* valor) {
  xSemaphoreTake(mtxAjustes, portMAX_DELAY);
  strncpy(destino, valor, n - 1);
  destino[n - 1] = 0;
  Preferences p;
  p.begin("receptor", false);
  p.putString(clave, destino);
  p.end();
  xSemaphoreGive(mtxAjustes);
}

static void ajustesGuardarWifi(const char* ssid, const char* clave) {
  ajustesGuardarTexto("ssid", ajustesActuales.ssid, sizeof(ajustesActuales.ssid), ssid);
  ajustesGuardarTexto("clave", ajustesActuales.clave, sizeof(ajustesActuales.clave), clave);
}

static void ajustesGuardarApiUrl(const char* v) {
  ajustesGuardarTexto("apiUrl", ajustesActuales.apiUrl, sizeof(ajustesActuales.apiUrl), v);
}
static void ajustesGuardarApiKey(const char* v) {
  ajustesGuardarTexto("apiKey", ajustesActuales.apiKey, sizeof(ajustesActuales.apiKey), v);
}
