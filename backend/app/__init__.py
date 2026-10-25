"""API REST: ESP32 receptor --HTTP POST--> API --> Firestore <-- dashboard."""

import hmac
from datetime import datetime, timedelta, timezone

from flask import Flask, jsonify, request
from flask_cors import CORS

from .config import cargar_config
from .storage import StorageError, crear_almacenamiento
from .validation import clave_deduplicacion, validar_lectura


def create_app(overrides=None, storage=None):
    config = cargar_config(overrides)
    app = Flask(__name__)
    app.config["MAX_CONTENT_LENGTH"] = 4096  # una lectura pesa < 300 bytes
    app.json.ensure_ascii = False

    origenes = [o.strip() for o in config["CORS_ORIGINS"].split(",") if o.strip()]
    CORS(app, origins=origenes or "*")

    db = storage or crear_almacenamiento(config)

    def autorizado():
        clave = config["API_KEY"]
        if not clave:
            return True
        enviada = request.headers.get("X-API-Key", "")
        return hmac.compare_digest(enviada.encode(), clave.encode())

    def parametro_entero(nombre, defecto, minimo, maximo):
        """Devuelve (valor, error). Rechaza texto no numérico en vez de ignorarlo."""
        crudo = request.args.get(nombre)
        if crudo is None or crudo == "":
            return defecto, None
        try:
            valor = int(crudo)
        except ValueError:
            return None, f"'{nombre}' debe ser un entero"
        return max(minimo, min(valor, maximo)), None

    @app.get("/health")
    def health():
        return jsonify(
            estado="ok",
            almacenamiento=db.nombre,
            autenticacion_escritura=bool(config["API_KEY"]),
            hora=datetime.now(timezone.utc).isoformat(),
        )

    @app.post("/api/lecturas")
    def crear_lectura():
        if not autorizado():
            return jsonify(error="API key inválida o ausente"), 401

        lectura, errores = validar_lectura(request.get_json(silent=True))
        if errores:
            return jsonify(error="Datos inválidos", detalles=errores), 400

        try:
            doc_id, creado = db.guardar(lectura, clave_deduplicacion(lectura))
        except StorageError:
            app.logger.exception("Fallo de almacenamiento")
            return jsonify(error="Almacenamiento no disponible"), 503
        if not creado:
            return jsonify(id=doc_id, duplicado=True, guardado=lectura), 200
        return jsonify(id=doc_id, duplicado=False, guardado=lectura), 201

    def consultar(limite, nodo, horas):
        desde = None
        if horas:
            desde = datetime.now(timezone.utc) - timedelta(hours=horas)
        try:
            return db.listar(limite, nodo, desde), None
        except StorageError:
            app.logger.exception("Fallo de consulta")
            return None, (jsonify(error="Almacenamiento no disponible"), 503)

    @app.get("/api/lecturas")
    def listar_lecturas():
        limite, err = parametro_entero("limit", 50, 1, config["MAX_LIMIT"])
        horas, err2 = parametro_entero("horas", 0, 0, 24 * 365)
        if err or err2:
            return jsonify(error="Parámetros inválidos", detalles=[e for e in (err, err2) if e]), 400
        resultado, fallo = consultar(limite, request.args.get("nodo"), horas)
        return fallo or jsonify(resultado)

    @app.get("/api/lecturas/ultima")
    def ultima_lectura():
        resultado, fallo = consultar(1, request.args.get("nodo"), 0)
        if fallo:
            return fallo
        if not resultado:
            return jsonify(error="No hay lecturas"), 404
        return jsonify(resultado[0])

    @app.get("/api/nodos")
    def listar_nodos():
        try:
            return jsonify(db.nodos())
        except StorageError:
            app.logger.exception("Fallo de consulta")
            return jsonify(error="Almacenamiento no disponible"), 503

    @app.errorhandler(404)
    def no_encontrado(_):
        return jsonify(error="Ruta no encontrada"), 404

    @app.errorhandler(405)
    def metodo_no_permitido(_):
        return jsonify(error="Método no permitido"), 405

    @app.errorhandler(413)
    def demasiado_grande(_):
        return jsonify(error="Cuerpo demasiado grande"), 413

    return app
