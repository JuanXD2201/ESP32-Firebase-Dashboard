# Firebase y Firestore

Proyecto académico indicado: **«Esp32»**. Su **ID técnico no se pudo verificar** en este entorno (Firebase CLI sin sesión);
cópialo de la consola: Configuración del proyecto → ID del proyecto, y úsalo abajo como `<ID_PROYECTO>`.

## Modelo de datos

Colección `lecturas`; ID = `nodo_sesion_seq` (con `sesion`) o automático.
Campos: `nodo` string · `seq` int · `temperatura` double · `humedad` double · `bombillo` bool · `rssi` int? · `snr` double? ·
`sesion` int? · `simulado` bool? · `creadoEn` timestamp (servidor).

Índice compuesto `nodo ASC, creadoEn DESC` en `firebase/firestore.indexes.json` (consultas por nodo, con o sin `horas`).

## Seguridad

`firebase/firestore.rules` deniega todo a clientes. La API usa Admin SDK (ignora reglas) con una **cuenta de servicio**.
El ESP32 y el navegador nunca reciben credenciales de Firebase.
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
