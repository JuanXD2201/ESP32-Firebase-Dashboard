"""Capa de almacenamiento.

- FirestoreStorage: guarda en una colección de Firestore (modo normal).
- MemoriaStorage: lista en RAM para pruebas sin Firebase (se pierde al reiniciar).

Se elige con la variable de entorno STORAGE (firestore | memoria).
Contrato común:
  guardar(lectura, doc_id=None) -> (id, creado)   creado=False si ya existía doc_id
  listar(limite, nodo=None, desde=None) -> lista, la más reciente primero
  nodos() -> lista de nodos vistos
  Cualquier fallo de almacenamiento lanza StorageError.
"""

import os
import uuid
from datetime import datetime, timezone


class StorageError(Exception):
    """El almacenamiento falló; la API responde 503 y nunca un éxito."""


def _iso(valor):
    return valor.isoformat() if isinstance(valor, datetime) else valor


class FirestoreStorage:
    nombre = "firestore"

    def __init__(self, coleccion, proyecto=None, credenciales=None):
        import firebase_admin
        from firebase_admin import credentials, firestore
        from google.api_core import exceptions as gexc

        self._firestore = firestore
        self._gexc = gexc
        if not firebase_admin._apps:
            cred = (
                credentials.Certificate(credenciales)
                if credenciales
                else credentials.ApplicationDefault()
            )
            opciones = {"projectId": proyecto} if proyecto else None
            firebase_admin.initialize_app(cred, opciones)
        self._col = firestore.client().collection(coleccion)

    def guardar(self, lectura, doc_id=None):
        # La hora de recepción la fija el servidor de Firestore (el ESP32 no tiene reloj).
        doc = dict(lectura, creadoEn=self._firestore.SERVER_TIMESTAMP)
        try:
            if doc_id:
                self._col.document(doc_id).create(doc)  # falla si ya existe
                return doc_id, True
            _, ref = self._col.add(doc)
            return ref.id, True
        except self._gexc.AlreadyExists:
            return doc_id, False
        except Exception as exc:  # noqa: BLE001 - se reenvía como error de almacenamiento
            raise StorageError(str(exc)) from exc

    def listar(self, limite, nodo=None, desde=None):
        consulta = self._col
        if nodo:
            consulta = consulta.where("nodo", "==", nodo)
        if desde:
            consulta = consulta.where("creadoEn", ">=", desde)
        consulta = consulta.order_by(
            "creadoEn", direction=self._firestore.Query.DESCENDING
        ).limit(limite)
        try:
            resultado = []
            for doc in consulta.stream():
                datos = doc.to_dict()
                datos["creadoEn"] = _iso(datos.get("creadoEn"))
                resultado.append(dict(datos, id=doc.id))
            return resultado
        except Exception as exc:  # noqa: BLE001
            raise StorageError(str(exc)) from exc

    def nodos(self):
        # Se deriva de las últimas lecturas: evita una colección extra y un índice.
        vistos = []
        for lectura in self.listar(200):
            if lectura["nodo"] not in vistos:
                vistos.append(lectura["nodo"])
        return vistos


class MemoriaStorage:
    nombre = "memoria"

    def __init__(self):
        self._datos = []

    def guardar(self, lectura, doc_id=None):
        if doc_id and any(d["id"] == doc_id for d in self._datos):
            return doc_id, False
        doc_id = doc_id or uuid.uuid4().hex[:20]
        self._datos.append(
            dict(lectura, id=doc_id, creadoEn=datetime.now(timezone.utc).isoformat())
        )
        return doc_id, True

    def listar(self, limite, nodo=None, desde=None):
        datos = [
            d
            for d in self._datos
            if (not nodo or d["nodo"] == nodo)
            and (not desde or d["creadoEn"] >= desde.isoformat())
        ]
        return list(reversed(datos))[:limite]

    def nodos(self):
        vistos = []
        for d in reversed(self._datos):
            if d["nodo"] not in vistos:
                vistos.append(d["nodo"])
        return vistos


def crear_almacenamiento(config, coleccion=None):
    if config["STORAGE"] == "memoria":
        return MemoriaStorage()
    credenciales = config["GOOGLE_APPLICATION_CREDENTIALS"] or None
    if credenciales and not os.path.exists(credenciales):
        raise RuntimeError(
            f"No existe el archivo de credenciales '{credenciales}'. Descarga la clave de "
            "la cuenta de servicio (ver docs/firebase.md), usa Application Default "
            "Credentials (gcloud auth application-default login) o STORAGE=memoria."
        )
    return FirestoreStorage(
        coleccion or config["FIRESTORE_COLLECTION"],
        proyecto=config["FIREBASE_PROJECT_ID"] or None,
        credenciales=credenciales,
    )
