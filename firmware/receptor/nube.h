// nube.h - envíos HTTP POST con tiempos de espera. Devuelve el código HTTP (>0) o un error
// negativo de HTTPClient. Nunca imprime claves.
#pragma once

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config.h"

// POST de un JSON. `respuesta` recibe el inicio del cuerpo (para el log).
static int nubePostJson(const char* url, const char* cabeceraClave, const char* valorClave,
                        const char* json, String* respuesta) {
  HTTPClient http;
  WiFiClientSecure seguro;
  WiFiClient plano;
  bool https = strncmp(url, "https://", 8) == 0;
  bool ok;
  if (https) {
    // Valida el certificado del servidor con el almacén de CA incluido en el core.
    extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
    seguro.setCACertBundle(x509_crt_bundle_start);
    ok = http.begin(seguro, url);
  } else {
    ok = http.begin(plano, url);
  }
  if (!ok) return HTTPC_ERROR_CONNECTION_REFUSED;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  if (cabeceraClave && valorClave && valorClave[0]) http.addHeader(cabeceraClave, valorClave);
  int codigo = http.POST((uint8_t*)json, strlen(json));
  if (codigo > 0 && respuesta) {
    String cuerpo = http.getString();
    *respuesta = cuerpo.substring(0, 80);
  }
  http.end();
  return codigo;
}
