#!/usr/bin/env bash
# Instala dependencias del backend y del frontend (Linux / macOS / Git Bash).
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"

echo "== Backend (Python) =="
cd "$RAIZ/backend"
python3 -m venv .venv
if [ -x .venv/bin/python ]; then PY=.venv/bin/python; else PY=.venv/Scripts/python; fi
"$PY" -m pip install -r requirements-dev.txt
[ -f .env ] || { cp "$RAIZ/.env.example" .env; echo "Creado backend/.env (edítalo)"; }

echo "== Frontend (Node 20.19+ / 22.12+) =="
cd "$RAIZ/frontend"
npm ci

echo "Listo. Pruebas: ./scripts/probar.sh"
