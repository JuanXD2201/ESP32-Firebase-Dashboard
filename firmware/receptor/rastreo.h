// rastreo.h - detección de duplicados y pérdidas por (nodo, sesion, seq). Sin Arduino: se
// prueba en el PC (tests/firmware/).
#pragma once

#include "protocolo.h"

#define RASTREO_NODOS_MAX 4
#define RASTREO_VENTANA_MAX 32

enum RastreoResultado { RASTREO_NUEVO, RASTREO_DUPLICADO };

class Rastreador {
 public:
  explicit Rastreador(uint32_t saltoMax = 1000, uint8_t ventana = 16)
      : saltoMax_(saltoMax), ventana_(ventana > RASTREO_VENTANA_MAX ? RASTREO_VENTANA_MAX : ventana) {
    memset(nodos_, 0, sizeof(nodos_));
  }

  // Evalúa una lectura válida. `perdidos` recibe los paquetes que faltaron antes de este.
  RastreoResultado evaluar(const LecturaLoRa& l, uint32_t* perdidos) {
    *perdidos = 0;
    Nodo* n = buscar(l.nodo);
    if (n->sesion != l.sesion || !n->activo) {  // nodo nuevo o emisor reiniciado
      n->activo = true;
      n->sesion = l.sesion;
      n->cantidad = 0;
      n->hayUltimo = false;
    }
    for (uint8_t i = 0; i < n->cantidad; i++) {
      if (n->recientes[i] == l.seq) return RASTREO_DUPLICADO;
    }
    if (n->hayUltimo && l.seq > n->ultimo + 1 && l.seq - n->ultimo - 1 <= saltoMax_) {
      *perdidos = l.seq - n->ultimo - 1;
    }
    if (n->cantidad < ventana_) {
      n->recientes[n->cantidad++] = l.seq;
    } else {
      memmove(n->recientes, n->recientes + 1, (ventana_ - 1) * sizeof(uint32_t));
      n->recientes[ventana_ - 1] = l.seq;
    }
    if (!n->hayUltimo || l.seq > n->ultimo) n->ultimo = l.seq;
    n->hayUltimo = true;
    return RASTREO_NUEVO;
  }

 private:
  struct Nodo {
    char nombre[PROTO_NODO_MAX + 1];
    bool activo;
    bool hayUltimo;
    uint32_t sesion;
    uint32_t ultimo;
    uint8_t cantidad;
    uint32_t recientes[RASTREO_VENTANA_MAX];
  };

  Nodo* buscar(const char* nombre) {
    for (auto& n : nodos_) {
      if (n.nombre[0] && strcmp(n.nombre, nombre) == 0) return &n;
    }
    for (auto& n : nodos_) {  // hueco libre
      if (!n.nombre[0]) {
        strncpy(n.nombre, nombre, PROTO_NODO_MAX);
        return &n;
      }
    }
    Nodo* n = &nodos_[reemplazo_++ % RASTREO_NODOS_MAX];  // tabla llena: se reutiliza
    memset(n, 0, sizeof(*n));
    strncpy(n->nombre, nombre, PROTO_NODO_MAX);
    return n;
  }

  uint32_t saltoMax_;
  uint8_t ventana_;
  uint8_t reemplazo_ = 0;
  Nodo nodos_[RASTREO_NODOS_MAX];
};
