# Instalación en otro computador

## Requisitos
Git · Python 3.12+ · Node 20.19+ o 22.12+ · Arduino CLI (firmware) · Firebase CLI (despliegue, `npm i -g firebase-tools`).
Drivers USB: CH9102 (emisor, wch.cn) y CP210x (receptor).

## Pasos
1. `git clone <URL> && cd ESP32-Firebase-Dashboard`
2. Dependencias: `.\scripts\instalar.ps1` (Windows) o `./scripts/instalar.sh`. Crea `backend/.venv`, instala todo y copia `.env.example` → `backend/.env`.
3. Variables: edita `backend/.env`. Por defecto `STORAGE=memoria` (sin Firebase). Para Firestore: ver `docs/firebase.md`.
4. Pruebas: `.\scripts\probar.ps1`. (Firmware en PC necesita un compilador C++: `pip install ziglang` o g++/clang++.)
5. API: `cd backend && .\.venv\Scripts\python -m app` → http://localhost:5000/health.
6. Datos de prueba sin hardware: `python scripts/simular_receptor.py http://localhost:5000` (marcados `simulado`).
7. Dashboard: edita `frontend/public/config.json` (`"apiUrl": "http://localhost:5000"`), `cd frontend && npm run dev`. Con `?demo=1` o el botón se activa el modo demostración (datos ficticios, rotulado).
8. Firmware: `.\scripts\compilar_firmware.ps1`; cargar desde Arduino IDE/CLI con los FQBN de `docs/conexiones_hardware.md`.
9. Configurar el receptor por el monitor serie (115200): `wifi`, `api http://<IP-PC>:5000`, `apikey <clave>`, `estado`.
   Si la API usa `API_KEY`, el mismo valor va en `apikey`.
10. Firebase y despliegue: `docs/firebase.md`, `docs/despliegue.md`.
11. Publicar cambios: `git add -A && git commit && git push` (revisa `git status`: nada de `.env`, claves ni `*.bin`).

Para que el ESP32 alcance la API en tu PC: misma red, permitir Python en el firewall de Windows (redes privadas), usar la IP del PC (no `localhost`).
