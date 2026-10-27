# Despliegue

**Estado:** proyecto cambiado a **`esp32-980d5`** (2026-10-27). La API ya escribe en su Firestore con la cuenta de servicio.
**Pendiente:** desplegar reglas/índices y el dashboard en este proyecto (`https://esp32-980d5.web.app`) y la conexión real de los ESP32.
Plan Spark: no hay Cloud Run; la API corre en un PC (`STORAGE=firestore`, `GOOGLE_APPLICATION_CREDENTIALS=<ruta del JSON>`).

## 1. Dashboard en Firebase Hosting

```bash
cd frontend && npm ci
# frontend/public/config.json:  {"apiUrl": "<URL pública de la API>", "urlDespliegue": "", "intervaloActualizacionS": 10}
npm run build            # genera firebase/public
cd ../firebase && firebase deploy --only hosting   # .firebaserc ya fija esp32-980d5
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

## 3. Publicar cambios en GitHub

`git add -A && git commit -m "..." && git push`. Antes: `git status` y revisa que no haya `.env`, claves ni `*.bin`.
