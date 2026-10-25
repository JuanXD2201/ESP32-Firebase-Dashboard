# Pruebas

Entorno: Windows 10, Python 3.12.8, Node 25.9.0, Arduino CLI 1.5.1, core esp32 2.0.12. Fecha: 2026-10-25.
Tipo: **A** automatizada · **S** simulada/mock · **F** física. Ninguna prueba física se ejecutó (placas no conectadas).

## Resultados ejecutados

| Prueba | Tipo | Comando | Resultado |
|---|---|---|---|
| API (contrato, validación, dedupe, 503) + adaptador Firestore con cliente **simulado** | A/S | `cd backend && .venv\Scripts\python -m pytest -q` | 30 passed |
| Dashboard (lógica, estados, URL, cliente API) | A | `cd frontend && npm test` | 14 passed |
| Build de producción del dashboard | A | `npm run build` | OK (vite 8.3.5) |
| Firmware: protocolo, checksum, rangos, JSON, duplicados/pérdidas, ThingSpeak JSON | A | `python tests/firmware/run_firmware_tests.py` (compilador zig c++) | 71 verificaciones, 0 fallos |
| Compilación emisor | A | `arduino-cli compile --fqbn esp32:esp32:ttgo-lora32:Revision=TTGO_LoRa32_v21new firmware/emisor` | OK, 83 % flash |
| Compilación receptor | A | `arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M firmware/receptor` | OK, 86 % flash |

| Integración local: API (`STORAGE=memoria`) + `scripts/simular_receptor.py` (6 POST, 201) + dashboard (`vite preview`) abierto en Chrome | S | manual | El dashboard mostró «API conectada», la lectura simulada con aviso «Valores SIMULADOS», historial y gráfico. **No** es Firestore real ni hardware. |

Una prueba de firmware falló en la primera ejecución por una expectativa incorrecta de la propia prueba (`9:abc`); se corrigió la prueba, no se relajó el código.

## No ejecutado (honestidad)

- **Firestore real** (escritura/lectura/dedupe): sin credenciales. Procedimiento: `scripts/probar_firestore.py` (`docs/firebase.md`).
- **Reglas de Firestore** con emulador (accesos autorizado/no autorizado): no ejecutado.
- **Despliegue** y verificación de la URL pública: no realizado (`docs/despliegue.md`).
- **Cloud Run / ThingSpeak reales**: no probados.
- `https://` desde el ESP32 (bundle de certificados): enlaza, sin probar en dispositivo.

## Verificación manual con hardware (pendiente)

1. Emisor: serie 9600; ver `[LORA] SX1276 listo`, `[SENSOR] T=… H=…` cada 5 s (sin `(SIMULADO)`), `[LORA] TX #n OK`. Tapar/desconectar el sensor ⇒ `lectura fallida` y **no** se transmite.
2. Receptor (115200): `[LORA] RX emisor-01 seq=n … rssi=… snr=…`. Con dos emisores/otro `G3_EMISOR` ⇒ `paquete descartado (formato)`.
3. Wi-Fi en cualquier momento: escribe `wifi`, SSID, clave ⇒ `[WIFI] conectado`. Repite con la app ya funcionando.
4. BLE (nRF Connect, servicio `6E400001-…`, escribir en `6E400002-…`): el serie muestra `[BLE] mensaje recibido` y el LED hace **3 × (250 ms ON / 250 ms OFF)**; medir con cronómetro/vídeo.
5. Nube: con `api` y `ts` configurados, `[API] OK HTTP 201` / `[THINGSPEAK] OK HTTP 200` y el LED hace **2 × (500 ms ON / 500 ms OFF)**. Con la API apagada: `FALLO` y **sin** parpadeo de éxito.
6. Reinicio del emisor: la `seq` vuelve a 0 con `sesion` +1 y el receptor no lo cuenta como duplicado.
7. Comandos BLE al emisor: `bombillo on|off|auto` ⇒ salida GPIO4 y 3 parpadeos del LED.
8. Dashboard: lecturas reales visibles con el estado «Datos recientes»; apagar el emisor ⇒ «Última lectura hace N min» mientras la API sigue «conectada».
