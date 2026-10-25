# API REST

Base: `http://localhost:5000` en local. Respuestas JSON en UTF-8. Escritura protegida por `X-API-Key` si `API_KEY` está definida.

| Método y ruta | Descripción | Respuestas |
|---|---|---|
| `GET /health` | Estado (`estado`, `almacenamiento`, `autenticacion_escritura`, `hora`) | 200 |
| `POST /api/lecturas` | Valida y guarda una lectura | 201 creada · 200 `duplicado:true` · 400 inválida · 401 clave · 413 >4 KB · 503 almacenamiento caído |
| `GET /api/lecturas?limit=50&nodo=emisor-01&horas=24` | Más reciente primero. `limit` 1–`MAX_LIMIT`(500), `horas` filtra por `creadoEn` | 200 · 400 parámetro no entero · 503 |
| `GET /api/lecturas/ultima?nodo=emisor-01` | Última lectura | 200 · 404 sin lecturas · 503 |
| `GET /api/nodos` | Nodos vistos recientemente | 200 · 503 |

## Cuerpo del POST

```json
{"nodo":"emisor-01","seq":42,"temperatura":23.4,"humedad":58.2,"bombillo":true,"rssi":-65,"snr":9.5,"sesion":3,"simulado":false}
```

Obligatorios: `nodo` (`[A-Za-z0-9_.-]{1,40}`), `seq` (entero ≥0), `temperatura` (−40…80), `humedad` (0…100), `bombillo` (bool o 0/1).
Opcionales: `rssi` (−200…0), `snr` (−30…30), `sesion` (entero ≥0, activa la deduplicación `nodo_sesion_seq`), `simulado` (bool).
Campos desconocidos se ignoran. `NaN`/`Infinity` se rechazan. Errores: `{"error":"Datos inválidos","detalles":["Falta el campo 'x'"]}`.

Respuesta 201: `{"id":"emisor-01_3_42","duplicado":false,"guardado":{...}}`. Cada documento guarda además `creadoEn` (hora del servidor, ISO 8601 UTC).

Los errores 503 no incluyen detalles internos (se registran en el log del servidor).
Las lecturas (GET) son públicas: contienen solo temperatura/humedad/estado; no pongas datos sensibles en la colección.
