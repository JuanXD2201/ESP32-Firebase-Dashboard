# Despliegue

**Estado:** dashboard desplegado en **https://esp32-8aa66.web.app** y reglas/índices de Firestore desplegados (2026-10-25).
**Pendiente:** la API que escribe en Firestore (necesita clave de cuenta de servicio) y la conexión real de los ESP32.
Plan Spark: no hay Cloud Run; la API corre en un PC (`STORAGE=firestore`, `GOOGLE_APPLICATION_CREDENTIALS=<ruta del JSON>`).

## 1. Dashboard en Firebase Hosting

```bash
cd frontend && npm ci
# frontend/public/config.json:  {"apiUrl": "<URL pública de la API>", "urlDespliegue": "", "intervaloActualizacionS": 10}
npm run build            # genera firebase/public
cd ../firebase && firebase deploy --only hosting   # .firebaserc ya fija esp32-8aa66
```

Hosting entrega `https://<ID_PROYECTO>.web.app`. El dashboard muestra esa URL **automáticamente** (usa `location.origin`
si no es local). Pega también la URL en `README.md` (sección «Aplicación web en producción») y en `urlDespliegue`.
Verifica abriendo la URL desde otro dispositivo y comprobando que la tabla muestra lecturas.

## 2. API en Cloud Run (necesita Blaze)

```bash
cd backend
gcloud run deploy esp32-api --source . --region us-central1 --allow-unauthenticated \
  --set-env-vars STORAGE=firestore,FIREBASE_PROJECT_ID=<ID_PROYECTO>,CORS_ORIGINS=https://<ID_PROYECTO>.web.app \
  --set-secrets API_KEY=esp32-api-key:latest
```

`--allow-unauthenticated` es necesario porque el ESP32 y el navegador llaman a la API; la escritura se protege con `X-API-Key`.
Concede a la cuenta de servicio de Cloud Run el rol «Cloud Datastore User». Deja `apiUrl` apuntando a la URL que devuelva el comando.

**Sin facturación**: ejecuta la API en un PC de la red del laboratorio (`python -m app`) y apunta el receptor a `http://<IP-PC>:5000/api/lecturas`;
el dashboard desplegado en Hosting no podrá alcanzarla (HTTPS→HTTP local), así que úsalo en local (`npm run dev`) con `apiUrl` local.

## 3. ThingSpeak (guía académica)

1. Crea cuenta y un canal con Field 1..N; copia la **Write API Key**.
2. En el monitor serie del receptor: `ts <WRITE_API_KEY>`.
3. Envía un mensaje por BLE (nRF Connect → servicio `6E400001-…` → característica `6E400002-…`): `23.5` → field1, `3:18.2` → field3, texto → status.
4. En ThingSpeak añade visualizaciones (gráficas/indicadores) al canal: ese es el «dashboard de ThingSpeak» que pide la guía.
Límite de la cuenta gratuita: 1 actualización cada 15 s (el firmware espera).

## 4. Publicar cambios en GitHub

`git add -A && git commit -m "..." && git push`. Antes: `git status` y revisa que no haya `.env`, claves ni `*.bin`.
