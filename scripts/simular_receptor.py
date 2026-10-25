"""Simula el POST del receptor hacia la API, SIN hardware. Los datos se marcan simulado=true.

Uso: python scripts/simular_receptor.py http://localhost:5000 [--clave API_KEY] [--n 5]
Sirve para probar API + Firestore + dashboard. NO prueba LoRa, BLE ni el sensor.
"""

import argparse
import json
import math
import sys
import time
import urllib.error
import urllib.request


def main():
    p = argparse.ArgumentParser()
    p.add_argument("api", help="URL base de la API, p. ej. http://localhost:5000")
    p.add_argument("--clave", default="", help="valor de X-API-Key si la API lo exige")
    p.add_argument("--n", type=int, default=5)
    p.add_argument("--nodo", default="sim-01")
    a = p.parse_args()

    sesion = int(time.time()) % 2**31  # sesión distinta en cada ejecución
    for i in range(a.n):
        cuerpo = {
            "nodo": a.nodo,
            "sesion": sesion,
            "seq": i,
            "temperatura": round(24 + 4 * math.sin(i / 3), 1),
            "humedad": round(55 + 10 * math.cos(i / 4), 1),
            "bombillo": False,
            "rssi": -70,
            "snr": 8.0,
            "simulado": True,
        }
        cabeceras = {"Content-Type": "application/json"}
        if a.clave:
            cabeceras["X-API-Key"] = a.clave
        req = urllib.request.Request(
            a.api.rstrip("/") + "/api/lecturas", json.dumps(cuerpo).encode(), cabeceras
        )
        try:
            with urllib.request.urlopen(req, timeout=10) as r:
                print(i, r.status, r.read().decode()[:100])
        except urllib.error.HTTPError as e:
            print(i, e.code, e.read().decode()[:200])
            return 1
        except urllib.error.URLError as e:
            print("No se pudo conectar:", e.reason)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
