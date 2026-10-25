"""Servidor de desarrollo: `python -m app` (producción: gunicorn wsgi:app)."""

from dotenv import load_dotenv

load_dotenv()

from . import create_app  # noqa: E402
from .config import cargar_config  # noqa: E402

if __name__ == "__main__":
    # 0.0.0.0: el ESP32 debe alcanzar la API desde la red local.
    create_app().run(host="0.0.0.0", port=cargar_config()["PORT"])
