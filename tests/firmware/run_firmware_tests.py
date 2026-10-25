"""Compila y ejecuta las pruebas de la lógica pura del firmware en el PC.

Busca un compilador C++ (g++, clang++, c++ o `python -m ziglang c++` si instalaste `ziglang`).
También verifica que protocolo.h sea idéntico en emisor y receptor.
Uso: python tests/firmware/run_firmware_tests.py
"""

import filecmp
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
FUENTE = Path(__file__).with_name("test_firmware.cpp")


def buscar_compilador():
    for nombre in ("g++", "clang++", "c++"):
        ruta = shutil.which(nombre)
        if ruta:
            return [ruta]
    try:
        import ziglang

        zig = Path(ziglang.__file__).parent / ("zig.exe" if sys.platform == "win32" else "zig")
        return [str(zig), "c++"] if zig.exists() else None
    except ImportError:
        return None


def main():
    emisor = RAIZ / "firmware" / "emisor" / "protocolo.h"
    receptor = RAIZ / "firmware" / "receptor" / "protocolo.h"
    if not filecmp.cmp(emisor, receptor, shallow=False):
        print("ERROR: protocolo.h difiere entre emisor y receptor")
        return 1
    print("OK: protocolo.h idéntico en emisor y receptor")

    compilador = buscar_compilador()
    if compilador is None:
        print("Sin compilador C++. Instala g++/clang++ o `pip install ziglang`.")
        return 2

    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "test_firmware.exe"
        cmd = [*compilador, "-std=c++17", "-Wall", "-Wextra", "-O1", str(FUENTE), "-o", str(exe)]
        compilacion = subprocess.run(cmd, capture_output=True, text=True)
        if compilacion.returncode != 0:
            print(compilacion.stdout + compilacion.stderr)
            return 1
        if compilacion.stderr.strip():
            print(compilacion.stderr)
        resultado = subprocess.run([str(exe)], capture_output=True, text=True)
        print(resultado.stdout + resultado.stderr, end="")
        return resultado.returncode


if __name__ == "__main__":
    sys.exit(main())
