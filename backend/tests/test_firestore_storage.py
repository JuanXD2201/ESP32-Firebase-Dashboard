"""FirestoreStorage con el cliente de Firestore simulado (NO es una prueba contra Firebase real).

Las pruebas contra Firebase real están en scripts/probar_firestore.py.
"""

import sys
import types
from datetime import datetime, timezone
from unittest.mock import MagicMock

import pytest

from app.storage import FirestoreStorage, StorageError


class AlreadyExists(Exception):
    pass


@pytest.fixture
def almacen(monkeypatch):
    firestore = types.SimpleNamespace(
        SERVER_TIMESTAMP="TS",
        Query=types.SimpleNamespace(DESCENDING="DESC"),
        client=MagicMock(),
    )
    admin = types.SimpleNamespace(_apps=[1], initialize_app=MagicMock())
    gexc = types.SimpleNamespace(AlreadyExists=AlreadyExists)
    fb = types.SimpleNamespace(
        firebase_admin=admin, credentials=MagicMock(), firestore=firestore
    )
    monkeypatch.setitem(sys.modules, "firebase_admin", admin)
    monkeypatch.setitem(sys.modules, "firebase_admin.credentials", fb.credentials)
    monkeypatch.setitem(sys.modules, "firebase_admin.firestore", firestore)
    monkeypatch.setitem(sys.modules, "google.api_core", types.SimpleNamespace(exceptions=gexc))
    monkeypatch.setitem(sys.modules, "google.api_core.exceptions", gexc)
    # `from firebase_admin import credentials, firestore` resuelve atributos del módulo:
    admin.credentials = fb.credentials
    admin.firestore = firestore
    return FirestoreStorage("lecturas"), firestore.client.return_value.collection.return_value


def test_guardar_con_id_usa_create_y_servidor_timestamp(almacen):
    s, col = almacen
    assert s.guardar({"nodo": "a"}, "a_1_2") == ("a_1_2", True)
    col.document.assert_called_with("a_1_2")
    col.document.return_value.create.assert_called_with({"nodo": "a", "creadoEn": "TS"})


def test_guardar_duplicado(almacen):
    s, col = almacen
    col.document.return_value.create.side_effect = AlreadyExists()
    assert s.guardar({"nodo": "a"}, "a_1_2") == ("a_1_2", False)


def test_guardar_sin_id_usa_add(almacen):
    s, col = almacen
    col.add.return_value = (None, types.SimpleNamespace(id="xyz"))
    assert s.guardar({"nodo": "a"}) == ("xyz", True)


def test_error_se_convierte_en_storage_error(almacen):
    s, col = almacen
    col.add.side_effect = RuntimeError("permission denied")
    with pytest.raises(StorageError):
        s.guardar({"nodo": "a"})


def test_listar_filtra_ordena_y_serializa_fecha(almacen):
    s, col = almacen
    doc = MagicMock(id="d1")
    doc.to_dict.return_value = {
        "nodo": "a",
        "creadoEn": datetime(2026, 1, 1, tzinfo=timezone.utc),
    }
    cadena = col.where.return_value.order_by.return_value.limit.return_value
    cadena.stream.return_value = [doc]
    r = s.listar(5, "a")
    col.where.assert_called_with("nodo", "==", "a")
    col.where.return_value.order_by.assert_called_with("creadoEn", direction="DESC")
    assert r == [{"nodo": "a", "creadoEn": "2026-01-01T00:00:00+00:00", "id": "d1"}]
