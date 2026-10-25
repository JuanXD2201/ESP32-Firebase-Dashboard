# Dashboard

Vite + JavaScript sin framework. Consulta la API definida en `public/config.json` (sin secretos):

```json
{"apiUrl": "https://<api>", "urlDespliegue": "", "intervaloActualizacionS": 10}
```

`npm ci` · `npm run dev` · `npm test` · `npm run build` (salida en `dist/`, desplegada por `firebase/firebase.json`).

- Sin `apiUrl` y sin demo: aviso de configuración (no se inventan datos).
- Modo demostración (`?demo=1` o botón): datos ficticios con nodo `DEMO`, aviso amarillo visible.
- «Aplicación web desplegada»: usa la URL real del navegador si no es local; si no, `urlDespliegue`; si no hay, «Aplicación pendiente de despliegue».
- Distingue conexión con la API de la antigüedad de la última lectura (obsoleta tras 120 s).
