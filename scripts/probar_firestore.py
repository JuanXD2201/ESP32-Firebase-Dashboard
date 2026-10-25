"""Prueba REAL contra Firestore (necesita credenciales; no se ejecuta en CI).

Uso (desde backend/ con el venv activo y .env configurado con STORAGE=firestore):
    python ../scripts/probar_firestore.py
Escribe un documento con nodo "prueba-firestore", lo lee, comprueba la deduplicación y lo borra.
"""

import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "backend"))
from dotenv import load_dotenv  # noqa: E402

load_dotenv(Path(__file__).resolve().parents[1] / "backend" / ".env")
os.environ["STORAGE"] = "firestore"

from app.config import cargar_config  # noqa: E402
from app.storage import crear_almacenamiento  # noqa: E402


def main():
    db = crear_almacenamiento(cargar_config())
    sesion = int(time.time())
    lectura = {
        "nodo": "prueba-firestore", "seq": 1, "sesion": sesion, "temperatura": 20.0,
        "humedad": 50.0, "bombillo": False, "simulado": True,
    }
    doc_id = f"prueba-firestore_{sesion}_1"
    assert db.guardar(lectura, doc_id) == (doc_id, True), "no se creó el documento"
    assert db.guardar(lectura, doc_id) == (doc_id, False), "no detectó el duplicado"
    leidas = db.listar(5, "prueba-firestore")
    assert leidas and leidas[0]["id"] == doc_id and leidas[0]["creadoEn"], leidas
    print("OK: escritura, deduplicación y consulta contra Firestore real")
    db._col.document(doc_id).delete()
    print("Documento de prueba eliminado")


if __name__ == "__main__":
    main()
