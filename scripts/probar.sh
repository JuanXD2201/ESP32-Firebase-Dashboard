#!/usr/bin/env bash
# Ejecuta todas las pruebas automatizadas disponibles (no requiere hardware ni Firebase).
set -uo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
fallos=0
if [ -x "$RAIZ/backend/.venv/bin/python" ]; then PY="$RAIZ/backend/.venv/bin/python"; else PY="$RAIZ/backend/.venv/Scripts/python"; fi

(cd "$RAIZ/backend" && "$PY" -m pytest -q) || fallos=$((fallos+1))
(cd "$RAIZ/frontend" && npm test && npm run build) || fallos=$((fallos+1))
"$PY" "$RAIZ/tests/firmware/run_firmware_tests.py"
rc=$?
if [ $rc -eq 2 ]; then echo "AVISO: pruebas de firmware omitidas (falta compilador C++)"; elif [ $rc -ne 0 ]; then fallos=$((fallos+1)); fi

[ $fallos -eq 0 ] && echo "Todas las pruebas ejecutadas pasaron." || { echo "$fallos grupo(s) fallaron"; exit 1; }
