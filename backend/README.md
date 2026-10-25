# Backend (Flask)

```powershell
python -m venv .venv ; .\.venv\Scripts\python -m pip install -r requirements-dev.txt
copy ..\.env.example .env          # edita STORAGE, FIREBASE_PROJECT_ID, API_KEY...
.\.venv\Scripts\python -m app      # desarrollo, :5000
.\.venv\Scripts\python -m pytest -q
```

Producción: `gunicorn wsgi:app` (ver `Dockerfile`). Contrato: `../docs/api.md`. Firestore: `../docs/firebase.md`.
`STORAGE=memoria` no persiste. Con `STORAGE=firestore` hace falta `FIREBASE_PROJECT_ID` y credenciales (clave de cuenta de servicio fuera del repo, o ADC).
