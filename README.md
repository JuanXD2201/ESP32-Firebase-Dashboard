# ESP32 · LoRa · BLE · Firebase · Dashboard

Proyecto académico (Seminario en TIC, guía «Capa de red en IoT»): dos ESP32 forman un enlace LoRa a 915 MHz;
el receptor es además pasarela Wi-Fi hacia una API REST que guarda en Firestore, y recibe mensajes BLE de un teléfono.
Un dashboard web muestra las lecturas.

```
AM2301 → emisor (T3 V1.6.1) ──LoRa 915 MHz──► receptor ──HTTP POST──► API Flask ──► Firestore
                  ▲ BLE (comandos)             ▲ BLE (mensajes) └─POST─► ThingSpeak         │
               teléfono                     teléfono                         dashboard ◄─GET─┘
```

## Aplicación web en producción

**https://esp32-8aa66.web.app** (Firebase Hosting, proyecto `esp32-8aa66`; verificado: HTTP 200 y carga con estado «API conectada»).
Lee Firestore directamente (solo lectura). Verificado de extremo a extremo con lecturas **simuladas** (API → Firestore → dashboard); luego se borraron.
La colección `lecturas` está vacía hasta que los ESP32 escriban a través de la API (ver `docs/firebase.md`).

URL pública de la API: **no existe** (plan Spark sin Cloud Run). La API se ejecuta en un PC de la red del laboratorio.

## Estado (verificado en este entorno)

| Componente | Estado |
|---|---|
| Firmware emisor | Compila (esp32 core 2.0.12). **Sin probar en hardware.** |
| Firmware receptor | Compila (esp32 core 2.0.12). **Sin probar en hardware.** |
| API REST | 30 pruebas pytest OK (memoria y Firestore **simulado**). Probada contra Firestore real (`scripts/probar_firestore.py` y POST reales). |
| Dashboard | 18 pruebas Vitest OK, build OK, **desplegado** y consultando Firestore real (vacío). |
| Firestore | Base creada, reglas e índice **desplegados**; verificado: lectura de `lecturas` 200, escritura sin credenciales 403, otra colección 403. |

Detalle de requisitos: [docs/matriz_requisitos.md](docs/matriz_requisitos.md). Pruebas físicas pendientes: [docs/pruebas.md](docs/pruebas.md).
Arquitectura y contrato de datos: [docs/arquitectura.md](docs/arquitectura.md).

## Estructura

```
firmware/emisor, firmware/receptor   sketches Arduino (config.h centraliza pines)
backend/                             API Flask + almacenamiento Firestore/memoria + pruebas
frontend/                            dashboard (Vite, JS sin framework) + pruebas
firebase/                            firebase.json, reglas e índices de Firestore
tests/firmware/                      pruebas en PC de protocolo, duplicados y mensajes BLE
scripts/                             instalar, probar, compilar firmware, simular, probar Firestore
docs/                                arquitectura, instalación, hardware, API, Firebase, despliegue, pruebas
```

## Inicio rápido (otro computador)

Requisitos: Git, Python 3.12+, Node 20.19+/22.12+, [Arduino CLI](https://arduino.github.io/arduino-cli/) (solo firmware).

```powershell
git clone https://github.com/JuanXD2201/ESP32-Firebase-Dashboard ; cd ESP32-Firebase-Dashboard
.\scripts\instalar.ps1            # Linux/macOS: ./scripts/instalar.sh
.\scripts\probar.ps1              # pruebas automatizadas
cd backend ; .\.venv\Scripts\python -m app          # API en :5000 (STORAGE=memoria por defecto)
cd frontend ; npm run dev                           # dashboard; apiUrl en frontend/public/config.json
.\scripts\compilar_firmware.ps1   # compila los dos sketches
```

Guía completa: [docs/instalacion.md](docs/instalacion.md).

## Seguridad

Sin credenciales en el repositorio. `.env`, claves de cuenta de servicio y `*.bin` están en `.gitignore`.
Firestore (`esp32-8aa66`): clientes solo pueden **leer** `lecturas` (el dashboard); toda escritura pasa por la API (Admin SDK). El ESP32 no lleva credenciales
administrativas; la clave `X-API-Key` y la de ThingSpeak se guardan en NVS por comandos serie, no en el código.
**No publiques `flash_com3.bin` / `flash_com4.bin`**: contienen redes Wi-Fi y contraseñas antiguas.

## Licencia

MIT — ver [LICENSE](LICENSE).
