# Arquitectura

```
sensor AM2301 (GPIO14)
   │ tareaSensor (≥2 s)
   ▼ cola
tareaLoRa ──LoRa 915 MHz, SF7/BW125/CR4-5, CRC──► tareaLoRa (receptor)
                                                     │ protoDecodificar + Rastreador
                                                     ▼ cola (20)
                                                  tareaRed ──POST JSON──► API Flask ──► Firestore ◄── GET ── dashboard
                                                     ▲ │                        (X-API-Key)
teléfono ──BLE (Nordic UART)──► callback BLE ──────┘ └─POST JSON──► ThingSpeak (field/status)
tareaLed: único dueño del LED   tareaSerie: Wi-Fi y ajustes en cualquier momento
```

## Canales (funciones diferenciadas)

- **LoRa**: emisor → receptor, lecturas del sensor.
- **BLE**: teléfono → receptor (mensajes, requisito de la guía) y teléfono → emisor (comandos del bombillo). No sustituye a LoRa.
- **Wi-Fi/HTTP**: receptor → API (lecturas) y receptor → ThingSpeak (mensajes BLE).

## Contrato LoRa (compacto, ASCII)

`nodo,sesion,seq,temp,hum,bombillo,simulado*CC` — ejemplo `emisor-01,3,42,23.4,58.2,1,0*1D` (≈40 bytes).
`CC` = XOR de los bytes anteriores a `*`, 2 hex mayúsculas. Un paquete que no cumple formato, rangos o checksum se
descarta y se cuenta (incluidos los de terceros como `G3_EMISOR`). Implementación: `firmware/*/protocolo.h`
(copias idénticas, verificadas por prueba).

| Campo | Tipo | Oblig. | Rango |
|---|---|---|---|
| nodo | `[A-Za-z0-9_.-]{1,24}` | sí | |
| sesion | uint32 | sí | contador de arranques del emisor (NVS) |
| seq | uint32 | sí | aumenta aunque falle la TX (deja hueco visible) |
| temp | float, 1 decimal | sí | −40…80 °C |
| hum | float, 1 decimal | sí | 0…100 % |
| bombillo | 0/1 | sí | estado de la salida |
| simulado | 0/1 | sí | 1 = valores generados, no medidos |

El receptor añade `rssi` (dBm) y `snr` (dB) **medidos** con `LoRa.packetRssi()/packetSnr()` y envía el JSON lógico:

```json
{"nodo":"emisor-01","sesion":3,"seq":42,"temperatura":23.4,"humedad":58.2,"bombillo":true,"simulado":false,"rssi":-65,"snr":9.5}
```

`sesion` y `simulado` son extensiones opcionales del esquema de referencia (`nodo, seq, temperatura, humedad, bombillo, rssi, snr`).
Firestore añade `creadoEn` (hora del servidor). Un sensor sin lectura válida **no genera paquete**.

## Duplicados y pérdidas

- Receptor: `Rastreador` recuerda las últimas 16 `(nodo, sesion, seq)`; duplicado ⇒ se ignora; hueco de `seq` ⇒ contador `perdidos`.
- API: con `sesion`, el ID del documento es `nodo_sesion_seq` y se crea con `create()`: reintentos del ESP32 son idempotentes (200 `duplicado:true`).
- Reinicio del emisor ⇒ nueva `sesion`, no se confunde con duplicado.

## FreeRTOS

Receptor: `tareaLoRa` (comunicaciones, prio 3, core 1), `tareaRed` (Wi-Fi+HTTP, prio 2, core 0), `tareaLed` (prio 1), `tareaSerie` (prio 1).
Sincronización: colas (`colaNube` 20, `colaLed` 8), notificación de tarea (credenciales nuevas), mutex del log y de los ajustes.
La cola llena descarta el trabajo más antiguo y lo cuenta. Wi-Fi se gestiona con máquina de estados sin esperas bloqueantes; HTTP con timeouts de 5 s y 3 reintentos.

## Decisiones técnicas

1. **Firestore solo vía API**; reglas deniegan a clientes. El dashboard consume la API (sin credenciales en el navegador).
2. **Flask**, compatible con el repo de referencia `MateoNaranjo/API-ESP32` (se reutilizó su contrato y se amplió: paquete `app/`, deduplicación, `/api/nodos`, filtro `horas`, 503 ante fallo de almacenamiento, `X-API-Key` en tiempo constante, límite de cuerpo).
3. **BLE y Wi-Fi simultáneos** en el receptor (la alternancia del Anexo 2 de la guía aplica a Bluetooth *clásico*). Sin probar en hardware.
4. **ThingSpeak** solo para los mensajes BLE (lo que pide la guía); Firebase sigue siendo el backend principal.
5. **Receptor compilado como «ESP32 Dev Module» con `min_spiffs`** porque BLE+Wi-Fi+TLS ocupan ≈1.7 MB (>1.3 MB del esquema por defecto). Las placas Heltec/TTGO del core asumen 8 MB o particiones pequeñas.
6. **Emisor sin Wi-Fi**: la guía lo exige en un único programa, que es el receptor; el emisor tiene BLE (comandos) pero no Wi-Fi.
7. **Dashboard sin framework** (Vite + JS) por el equipo de 4 GB de RAM.
