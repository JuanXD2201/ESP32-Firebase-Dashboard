"""Configuración leída de variables de entorno (ver .env.example)."""

import os


def cargar_config(overrides=None):
    config = {
        "STORAGE": os.getenv("STORAGE", "firestore").lower(),
        "GOOGLE_APPLICATION_CREDENTIALS": os.getenv("GOOGLE_APPLICATION_CREDENTIALS", ""),
        "FIREBASE_PROJECT_ID": os.getenv("FIREBASE_PROJECT_ID", ""),
        "FIRESTORE_COLLECTION": os.getenv("FIRESTORE_COLLECTION", "lecturas"),
        # Si se define, POST /api/lecturas exige el header X-API-Key con este valor.
        "API_KEY": os.getenv("API_KEY", ""),
        # Orígenes permitidos para CORS (separados por coma). "*" = cualquiera.
        "CORS_ORIGINS": os.getenv("CORS_ORIGINS", "*"),
        "MAX_LIMIT": int(os.getenv("MAX_LIMIT", "500")),
        "PORT": int(os.getenv("PORT", "5000")),
    }
    config.update(overrides or {})
    return config
