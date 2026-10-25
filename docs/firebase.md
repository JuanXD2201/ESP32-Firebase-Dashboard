# Firebase y Firestore

Proyecto: **«Esp32»**, ID técnico **`esp32-8aa66`** (verificado en la consola, plan Spark). `firebase/.firebaserc` ya lo fija.
Plan Spark ⇒ sin Cloud Run: el dashboard desplegado en Hosting **lee Firestore directamente** (REST, solo lectura) y las escrituras
las hace la API ejecutada en un PC con cuenta de servicio.
Estado de la base: la creación de la base `(default)` se inició en la consola; **falta elegir la ubicación** (irreversible) y confirmar.

## Modelo de datos

Colección `lecturas`; ID = `nodo_sesion_seq` (con `sesion`) o automático.
Campos: `nodo` string · `seq` int · `temperatura` double · `humedad` double · `bombillo` bool · `rssi` int? · `snr` double? ·
`sesion` int? · `simulado` bool? · `creadoEn` timestamp (servidor).

Índice compuesto `nodo ASC, creadoEn DESC` en `firebase/firestore.indexes.json` (consultas por nodo, con o sin `horas`).

## Seguridad

`firebase/firestore.rules`: **lectura pública solo de `lecturas`** (límite 500 por consulta), **escritura denegada a todos los clientes**,
todo lo demás denegado. Decisión consciente por el plan Spark: no pongas datos sensibles en `lecturas`.
La API usa Admin SDK (ignora reglas) con una **cuenta de servicio**. El ESP32 y el navegador nunca reciben credenciales de Firebase.
Las consultas del dashboard con filtro de nodo necesitan el índice compuesto de `firestore.indexes.json` (se despliega con `firebase deploy --only firestore`).
Verificación de reglas con emulador: **pendiente** (`firebase emulators:exec --only firestore` + pruebas de acceso denegado).

## Puesta en marcha (requiere tu cuenta)

```bash
npm i -g firebase-tools
firebase login                                   # abre el navegador; concede permisos tú mismo
firebase projects:list                           # confirma el ID técnico de «Esp32»
cd firebase
firebase use <ID_PROYECTO>                       # crea .firebaserc
firebase deploy --only firestore                 # reglas + índices (en la consola crea antes la base de datos en modo producción)
```

Credenciales para la API (elige una, nunca las subas a git):
1. Local: consola → Configuración → Cuentas de servicio → Generar clave privada; guarda el JSON fuera del repo y pon su ruta en `GOOGLE_APPLICATION_CREDENTIALS`.
2. Cloud Run: la cuenta de servicio del servicio con rol `Cloud Datastore User` (sin archivo de clave).

Prueba real: `cd backend && .venv/Scripts/python ../scripts/probar_firestore.py` con `FIREBASE_PROJECT_ID` y credenciales configurados.
