"""Pruebas de contrato de la API con almacenamiento en memoria o simulado."""

import pytest

from app import create_app
from app.storage import MemoriaStorage, StorageError

LECTURA = {
    "nodo": "emisor-01",
    "seq": 42,
    "temperatura": 23.4,
    "humedad": 58.2,
    "bombillo": True,
    "rssi": -65,
    "snr": 9.5,
}


@pytest.fixture
def cliente():
    app = create_app({"STORAGE": "memoria", "API_KEY": ""}, storage=MemoriaStorage())
    return app.test_client()


@pytest.fixture
def cliente_con_clave():
    app = create_app({"STORAGE": "memoria", "API_KEY": "secreta"}, storage=MemoriaStorage())
    return app.test_client()


class AlmacenFallido:
    nombre = "roto"

    def guardar(self, *a, **k):
        raise StorageError("boom con detalles internos")

    listar = nodos = guardar


def test_health(cliente):
    r = cliente.get("/health")
    assert r.status_code == 200
    assert r.json["estado"] == "ok"
    assert r.json["almacenamiento"] == "memoria"
    assert "secreta" not in r.get_data(as_text=True)


def test_post_valido_devuelve_201(cliente):
    r = cliente.post("/api/lecturas", json=LECTURA)
    assert r.status_code == 201
    assert r.json["duplicado"] is False
    assert r.json["guardado"]["temperatura"] == 23.4


@pytest.mark.parametrize(
    "cambio",
    [
        {"temperatura": 200},
        {"humedad": -1},
        {"humedad": "mucha"},
        {"seq": -3},
        {"seq": 1.5},
        {"bombillo": "si"},
        {"nodo": ""},
        {"nodo": "con espacio"},
        {"rssi": 5},
    ],
)
def test_post_invalido_devuelve_400(cliente, cambio):
    r = cliente.post("/api/lecturas", json={**LECTURA, **cambio})
    assert r.status_code == 400
    assert r.json["detalles"]


def test_post_nan_se_rechaza(cliente):
    cuerpo = '{"nodo":"a","seq":1,"temperatura":NaN,"humedad":50,"bombillo":true}'
    r = cliente.post("/api/lecturas", data=cuerpo, content_type="application/json")
    assert r.status_code == 400


def test_post_campos_obligatorios(cliente):
    r = cliente.post("/api/lecturas", json={"nodo": "x"})
    assert r.status_code == 400
    assert len(r.json["detalles"]) >= 4


def test_post_sin_json(cliente):
    assert cliente.post("/api/lecturas", data="no json").status_code == 400
    assert cliente.post("/api/lecturas", json=[1, 2]).status_code == 400


def test_post_demasiado_grande(cliente):
    r = cliente.post("/api/lecturas", json={**LECTURA, "relleno": "x" * 5000})
    assert r.status_code == 413


def test_rssi_snr_opcionales(cliente):
    sin = {k: v for k, v in LECTURA.items() if k not in ("rssi", "snr")}
    assert cliente.post("/api/lecturas", json=sin).status_code == 201


def test_api_key(cliente_con_clave):
    assert cliente_con_clave.post("/api/lecturas", json=LECTURA).status_code == 401
    mala = cliente_con_clave.post("/api/lecturas", json=LECTURA, headers={"X-API-Key": "x"})
    assert mala.status_code == 401
    ok = cliente_con_clave.post("/api/lecturas", json=LECTURA, headers={"X-API-Key": "secreta"})
    assert ok.status_code == 201
    # La lectura es pública (la usa el dashboard); la escritura no.
    assert cliente_con_clave.get("/api/lecturas").status_code == 200


def test_duplicado_con_sesion_es_idempotente(cliente):
    cuerpo = {**LECTURA, "sesion": 3}
    assert cliente.post("/api/lecturas", json=cuerpo).status_code == 201
    r = cliente.post("/api/lecturas", json=cuerpo)
    assert r.status_code == 200
    assert r.json["duplicado"] is True
    assert len(cliente.get("/api/lecturas").json) == 1


def test_misma_seq_otra_sesion_no_es_duplicado(cliente):
    cliente.post("/api/lecturas", json={**LECTURA, "sesion": 1})
    r = cliente.post("/api/lecturas", json={**LECTURA, "sesion": 2})
    assert r.status_code == 201


def test_listar_orden_limite_y_filtro(cliente):
    for i in range(5):
        cliente.post("/api/lecturas", json={**LECTURA, "seq": i, "sesion": 1})
    cliente.post("/api/lecturas", json={**LECTURA, "nodo": "otro", "seq": 0})

    r = cliente.get("/api/lecturas?limit=3&nodo=emisor-01")
    assert r.status_code == 200
    assert [x["seq"] for x in r.json] == [4, 3, 2]
    assert len(cliente.get("/api/lecturas").json) == 6
    assert len(cliente.get("/api/lecturas?nodo=otro").json) == 1


def test_limit_se_acota_y_se_valida(cliente):
    assert cliente.get("/api/lecturas?limit=0").status_code == 200  # se acota a 1
    assert cliente.get("/api/lecturas?limit=abc").status_code == 400
    assert cliente.get("/api/lecturas?horas=x").status_code == 400


def test_ultima(cliente):
    assert cliente.get("/api/lecturas/ultima?nodo=emisor-01").status_code == 404
    cliente.post("/api/lecturas", json={**LECTURA, "seq": 1})
    cliente.post("/api/lecturas", json={**LECTURA, "seq": 2})
    r = cliente.get("/api/lecturas/ultima?nodo=emisor-01")
    assert r.status_code == 200
    assert r.json["seq"] == 2


def test_nodos(cliente):
    cliente.post("/api/lecturas", json=LECTURA)
    cliente.post("/api/lecturas", json={**LECTURA, "nodo": "b"})
    assert set(cliente.get("/api/nodos").json) == {"emisor-01", "b"}


def test_ruta_inexistente_y_metodo(cliente):
    assert cliente.get("/nada").status_code == 404
    assert cliente.delete("/api/lecturas").status_code == 405


def test_fallo_de_almacenamiento_no_devuelve_exito():
    app = create_app({"STORAGE": "memoria", "API_KEY": ""}, storage=AlmacenFallido())
    c = app.test_client()
    r = c.post("/api/lecturas", json=LECTURA)
    assert r.status_code == 503
    assert "boom" not in r.get_data(as_text=True)  # no filtra detalles internos
    assert c.get("/api/lecturas").status_code == 503
    assert c.get("/api/lecturas/ultima").status_code == 503
    assert c.get("/api/nodos").status_code == 503


# ---- mensajes BLE (reemplazan a ThingSpeak: el dashboard los muestra) ----
MENSAJE = {"nodo": "receptor-01", "texto": "hola desde el celular"}


def test_mensaje_valido_y_listado(cliente):
    r = cliente.post("/api/mensajes", json=MENSAJE)
    assert r.status_code == 201
    assert r.json["guardado"] == {**MENSAJE, "origen": "ble"}
    cliente.post("/api/mensajes", json={**MENSAJE, "texto": "segundo"})
    lista = cliente.get("/api/mensajes?limit=5").json
    assert [m["texto"] for m in lista] == ["segundo", "hola desde el celular"]
    # los mensajes no se mezclan con las lecturas
    assert cliente.get("/api/lecturas").json == []


@pytest.mark.parametrize(
    "cuerpo",
    [
        {"texto": "sin nodo"},
        {"nodo": "r"},
        {"nodo": "r", "texto": "   "},
        {"nodo": "r", "texto": "x" * 101},
        {"nodo": "r", "texto": "con\ncontrol"},
        {"nodo": "r", "texto": 5},
        {"nodo": "mal nodo", "texto": "x"},
    ],
)
def test_mensaje_invalido(cliente, cuerpo):
    assert cliente.post("/api/mensajes", json=cuerpo).status_code == 400


def test_mensaje_exige_api_key(cliente_con_clave):
    assert cliente_con_clave.post("/api/mensajes", json=MENSAJE).status_code == 401
    ok = cliente_con_clave.post("/api/mensajes", json=MENSAJE, headers={"X-API-Key": "secreta"})
    assert ok.status_code == 201


def test_mensajes_fallo_de_almacenamiento():
    app = create_app({"STORAGE": "memoria", "API_KEY": ""}, storage_mensajes=AlmacenFallido())
    c = app.test_client()
    assert c.post("/api/mensajes", json=MENSAJE).status_code == 503
    assert c.get("/api/mensajes").status_code == 503
