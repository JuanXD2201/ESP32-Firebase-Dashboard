# Matriz de requisitos

Fuente: guía «Práctica No. 1 – Capa de red en IoT» (PDF original, leído completo) y `PROMPT_MAESTRO_ESP32.md`.
Estados: **Implementado** (código existe y compila) · **Probado SW** (prueba automatizada en PC) ·
**Pendiente HW** (requiere placas conectadas) · **Bloqueado** (falta credencial/permiso).
Ningún requisito está marcado «aprobado en hardware»: las placas no estaban conectadas.

## A. Condiciones funcionales de la guía (se evalúan juntas en un mismo código: `firmware/receptor/receptor.ino`)

| # | Requisito de la guía | Archivo · función | Prueba | Estado |
|---|---|---|---|---|
| 1 | SSID y contraseña Wi-Fi por monitor serie «en cualquier momento» | `receptor.ino` · `tareaSerie`, `procesarLinea` (comando `wifi`) | Compila; lógica no automatizada | Implementado · Pendiente HW |
| 2 | Recibir datos del celular por BLE y mostrarlos en serie | `receptor.ino` · `CallbackEscritura::onWrite` | Compila | Implementado · Pendiente HW |
| 3 | LED: 3 parpadeos, 0.25 s ON / 0.25 s OFF al recibir BLE | `receptor.ino` · `tareaLed` → `parpadear(3,250,250)` | Compila | Implementado · Pendiente HW |
| 4 | HTTP POST a ThingSpeak con los datos recibidos (JSON) | `receptor.ino` · `enviarTrabajo`; `thingspeak.h` · `tsConstruirJson`; `nube.h` | `test_firmware.cpp::thingspeak_mensajes_ble` (JSON) | Implementado · Probado SW (JSON) · Pendiente HW/clave ThingSpeak |
| 5 | LED: 2 parpadeos, 0.5 s ON / 0.5 s OFF al éxito de la nube | `receptor.ino` · `tareaRed` → `LED_NUBE_OK` → `parpadear(2,500,500)` | Compila | Implementado · Pendiente HW |
| 6 | FreeRTOS con tareas separadas (BLE, Wi-Fi, LED) | `receptor.ino` · `tareaLoRa`, `tareaRed`, `tareaLed`, `tareaSerie` + callback BLE | Compila | Implementado · Pendiente HW |
| — | Dashboard en la interfaz de ThingSpeak (objetivo 4 de «Problema de diseño») | Configuración manual en thingspeak.com | — | **Bloqueado**: requiere cuenta/canal del usuario (pasos en `docs/despliegue.md`) |

Notas: la guía señala que sus pruebas «deben funcionar en conjunto en un mismo código»; el receptor las reúne.
Éxito de la nube = HTTP 200/201 (ThingSpeak: además cuerpo ≠ `0`). El receptor envía a ThingSpeak solo los mensajes BLE; Firebase es el backend principal.

## B. Entregables del prompt maestro

| ID | Requisito | Archivo | Prueba | Estado |
|---|---|---|---|---|
| E1 | Emisor: AM2301 ≥2 s, LoRa 915 MHz, id y secuencia, FreeRTOS, BLE→LED 3×250 ms | `firmware/emisor/emisor.ino`, `config.h` | Compila (83 % flash); protocolo en PC | Implementado · Probado SW (protocolo) · Pendiente HW |
| E1 | Wi-Fi por serie en el **emisor** (texto de E1) | — | — | **No implementado por diseño**: la guía lo pide en el programa que envía a la nube (receptor); ver `arquitectura.md` §6 |
| E1 | Modo simulación explícito | `emisor.ino` (`sim on/off`, `SIMULACION_AL_ARRANCAR`) | `simulado` viaja en trama/API/dashboard | Implementado · Probado SW (campo) |
| E2 | Receptor: LoRa, validación, JSON, HTTP, duplicados, reconexión | `receptor.ino`, `protocolo.h`, `rastreo.h`, `nube.h` | `test_firmware.cpp` (checksum, rangos, JSON, duplicados) | Implementado · Probado SW · Pendiente HW |
| E2 | Fallos HTTP no se reportan como éxito | `enviarTrabajo` (código ≠200/201 ⇒ error) | Revisión de código; sin prueba automática | Implementado · Pendiente HW |
| E3 | API: `/health`, `POST /api/lecturas`, `GET /api/lecturas`, `GET /api/lecturas/ultima` | `backend/app/__init__.py` | `backend/tests/test_api.py` (30 pruebas) | Implementado · Probado SW |
| E3 | No devuelve éxito si el almacenamiento falla | `crear_lectura` → 503 | `test_fallo_de_almacenamiento_no_devuelve_exito` | Probado SW |
| E4 | Estructura Firestore, reglas, integración real | `backend/app/storage.py`, `firebase/*` | `test_firestore_storage.py` (cliente **simulado**) | Implementado · Reglas/índice desplegados y verificados · Escritura real **bloqueada** (falta clave de cuenta de servicio) |
| E4 | Reglas sin acceso público indiscriminado | `firebase/firestore.rules` (lectura solo de `lecturas`, escritura denegada) | curl real: lectura 200, escritura 403 | Probado (Firebase real) |
| E5 | Dashboard con estados de carga/vacío/error, historial, filtros, estado del sistema | `frontend/src/*` | `frontend/tests/logica.test.js` (14), `vite build` | Implementado · Probado SW · Datos reales no verificados |
| E5 | URL pública en la interfaz y README | `main.js::pintarDespliegue`, `README.md` | Prueba de `resolverUrlDespliegue` | Implementado · Desplegado: https://esp32-8aa66.web.app (datos reales: colección vacía) |
| E6 | Diagrama y contratos documentados | `docs/arquitectura.md` | — | Implementado |
| E7 | Pruebas y evidencias | `docs/pruebas.md` | — | Implementado |
| E8 | Repo portable, `.env.example`, `.gitignore`, scripts | raíz, `scripts/` | Auditoría de secretos | Implementado |
| — | ThingSpeak configurable sin sustituir Firebase | `receptor.ino` (`ts <clave>`) | JSON probado | Implementado · Pendiente HW |
