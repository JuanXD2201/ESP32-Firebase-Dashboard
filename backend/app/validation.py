"""Validación del esquema lógico de una lectura.

Esquema (campo: tipo, obligatorio, mínimo, máximo):
  nodo         str    sí  identificador del nodo emisor (1-40 caracteres)
  seq          int    sí  número de secuencia >= 0
  temperatura  float  sí  -40 a 80 °C (rango del AM2301/DHT21)
  humedad      float  sí  0 a 100 %
  bombillo     bool   sí  estado de la salida
  rssi         int    no  -200 a 0 dBm (medido por el receptor)
  snr          float  no  -30 a 30 dB (medido por el receptor)
  sesion       int    no  contador de arranques del emisor; con `seq` forma la
                          clave de deduplicación
  simulado     bool   no  true si el emisor estaba en modo simulación
"""

import math
import re

NODO_RE = re.compile(r"^[A-Za-z0-9_.\-]{1,40}$")

# campo: (tipo, obligatorio, mínimo, máximo)
ESQUEMA = {
    "nodo": (str, True, None, None),
    "seq": (int, True, 0, 2**31 - 1),
    "temperatura": (float, True, -40.0, 80.0),
    "humedad": (float, True, 0.0, 100.0),
    "bombillo": (bool, True, None, None),
    "rssi": (int, False, -200, 0),
    "snr": (float, False, -30.0, 30.0),
    "sesion": (int, False, 0, 2**31 - 1),
    "simulado": (bool, False, None, None),
}


def _convertir(campo, tipo, valor):
    """Devuelve (valor_convertido, error)."""
    if tipo is bool:
        if isinstance(valor, bool):
            return valor, None
        if isinstance(valor, int) and valor in (0, 1):
            return bool(valor), None
        return None, f"'{campo}' debe ser booleano (true/false o 1/0)"
    if tipo is int:
        if isinstance(valor, bool) or not isinstance(valor, int):
            return None, f"'{campo}' debe ser entero"
        return valor, None
    if tipo is float:
        if isinstance(valor, bool) or not isinstance(valor, (int, float)):
            return None, f"'{campo}' debe ser numérico"
        valor = float(valor)
        if math.isnan(valor) or math.isinf(valor):
            return None, f"'{campo}' no es un número válido"
        return valor, None
    if not isinstance(valor, str) or not valor.strip():
        return None, f"'{campo}' debe ser texto no vacío"
    valor = valor.strip()
    if campo == "nodo" and not NODO_RE.match(valor):
        return None, "'nodo' solo admite letras, números, '_', '-' y '.' (máx 40)"
    return valor, None


def validar_lectura(datos):
    """Devuelve (lectura_limpia, errores). Los campos desconocidos se ignoran."""
    if not isinstance(datos, dict):
        return None, ["El cuerpo debe ser un objeto JSON"]

    errores = []
    lectura = {}
    for campo, (tipo, obligatorio, minimo, maximo) in ESQUEMA.items():
        if datos.get(campo) is None:
            if obligatorio:
                errores.append(f"Falta el campo '{campo}'")
            continue

        valor, error = _convertir(campo, tipo, datos[campo])
        if error:
            errores.append(error)
            continue
        if minimo is not None and valor < minimo:
            errores.append(f"'{campo}' fuera de rango (mín {minimo})")
            continue
        if maximo is not None and valor > maximo:
            errores.append(f"'{campo}' fuera de rango (máx {maximo})")
            continue
        lectura[campo] = valor

    return (None, errores) if errores else (lectura, [])


def clave_deduplicacion(lectura):
    """ID de documento determinista, o None si no hay `sesion` (sin deduplicar)."""
    if "sesion" not in lectura:
        return None
    return f"{lectura['nodo']}_{lectura['sesion']}_{lectura['seq']}"


def validar_mensaje(datos):
    """Mensaje recibido por BLE en el receptor. Devuelve (mensaje_limpio, errores).

    nodo   str  sí  identificador del receptor (mismas reglas que en una lectura)
    texto  str  sí  1-100 caracteres; los caracteres de control se rechazan
    """
    if not isinstance(datos, dict):
        return None, ["El cuerpo debe ser un objeto JSON"]
    errores = []
    nodo, error = (None, "Falta el campo 'nodo'")
    if datos.get("nodo") is not None:
        nodo, error = _convertir("nodo", str, datos["nodo"])
    if error:
        errores.append(error)
    texto = datos.get("texto")
    if texto is None:
        errores.append("Falta el campo 'texto'")
    elif not isinstance(texto, str) or not texto.strip():
        errores.append("'texto' debe ser texto no vacío")
    else:
        texto = texto.strip()
        if len(texto) > 100:
            errores.append("'texto' fuera de rango (máx 100 caracteres)")
        elif any(ord(c) < 32 or ord(c) == 127 for c in texto):
            errores.append("'texto' no admite caracteres de control")
    if errores:
        return None, errores
    return {"nodo": nodo, "texto": texto, "origen": "ble"}, []
