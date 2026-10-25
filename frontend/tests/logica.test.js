import { describe, expect, it, vi } from "vitest";
import { crearCliente, ErrorApi } from "../src/api.js";
import {
  antiguedadSegundos,
  escaparHtml,
  estadoSistema,
  formatearNumero,
  generarDemo,
  resolverUrlDespliegue,
  textoAntiguedad,
} from "../src/logica.js";

const AHORA = Date.parse("2026-10-25T12:00:00Z");
const hace = (s) => new Date(AHORA - s * 1000).toISOString();

describe("formato", () => {
  it("formatea números y valores ausentes", () => {
    expect(formatearNumero(23.456)).toBe("23.5");
    expect(formatearNumero(null)).toBe("—");
    expect(formatearNumero(NaN)).toBe("—");
  });
  it("textoAntiguedad", () => {
    expect(textoAntiguedad(5)).toBe("hace 5 s");
    expect(textoAntiguedad(125)).toBe("hace 2 min");
    expect(textoAntiguedad(7200)).toBe("hace 2 h");
    expect(textoAntiguedad(null)).toBe("sin datos");
  });
  it("antiguedadSegundos tolera fechas inválidas", () => {
    expect(antiguedadSegundos("basura")).toBeNull();
    expect(antiguedadSegundos(hace(30), AHORA)).toBe(30);
  });
  it("escapa HTML", () => {
    expect(escaparHtml('<img onerror="x">')).toBe("&lt;img onerror=&quot;x&quot;&gt;");
  });
});

describe("estadoSistema", () => {
  it("separa conexión de datos", () => {
    const reciente = estadoSistema({ apiOk: true, cargando: false, lectura: { creadoEn: hace(10) }, ahora: AHORA });
    expect(reciente.conexion.clase).toBe("ok");
    expect(reciente.datos.clase).toBe("ok");

    const obsoleta = estadoSistema({ apiOk: true, cargando: false, lectura: { creadoEn: hace(600) }, ahora: AHORA });
    expect(obsoleta.conexion.clase).toBe("ok");
    expect(obsoleta.datos.clase).toBe("obsoleto");

    const caida = estadoSistema({ apiOk: false, cargando: false, lectura: { creadoEn: hace(10) }, ahora: AHORA });
    expect(caida.conexion.clase).toBe("error");
  });
  it("estados de carga y vacío", () => {
    expect(estadoSistema({ apiOk: null, cargando: true, lectura: undefined }).conexion.clase).toBe("cargando");
    expect(estadoSistema({ apiOk: true, cargando: false, lectura: undefined }).datos.clase).toBe("vacio");
  });
});

describe("resolverUrlDespliegue", () => {
  it("usa el origen real si no es local", () => {
    expect(resolverUrlDespliegue({ hostname: "esp32.web.app", origin: "https://esp32.web.app" }, "")).toBe(
      "https://esp32.web.app",
    );
  });
  it("nunca muestra localhost ni IP privadas", () => {
    expect(resolverUrlDespliegue({ hostname: "localhost", origin: "http://localhost:5173" }, "")).toBeNull();
    expect(resolverUrlDespliegue({ hostname: "192.168.1.5", origin: "http://192.168.1.5" }, "")).toBeNull();
    expect(resolverUrlDespliegue({ hostname: "localhost" }, "http://127.0.0.1:5000")).toBeNull();
  });
  it("usa la URL configurada cuando la página es local", () => {
    expect(resolverUrlDespliegue({ hostname: "localhost" }, "https://x.web.app")).toBe("https://x.web.app");
  });
});

describe("demo", () => {
  it("genera filas marcadas como simuladas, más reciente primero", () => {
    const filas = generarDemo(5, AHORA);
    expect(filas).toHaveLength(5);
    expect(filas.every((f) => f.simulado && f.nodo === "DEMO")).toBe(true);
    expect(new Date(filas[0].creadoEn) > new Date(filas[4].creadoEn)).toBe(true);
  });
});

describe("cliente API", () => {
  const respuesta = (cuerpo, status = 200) => ({ ok: status < 400, status, json: async () => cuerpo });

  it("construye la consulta con filtros", async () => {
    const f = vi.fn().mockResolvedValue(respuesta([]));
    await crearCliente("https://api.test", f).lecturas({ limit: 10, nodo: "emisor-01", horas: 6 });
    expect(String(f.mock.calls[0][0])).toBe("https://api.test/api/lecturas?limit=10&nodo=emisor-01&horas=6");
  });
  it("omite filtros vacíos", async () => {
    const f = vi.fn().mockResolvedValue(respuesta([]));
    await crearCliente("https://api.test/", f).lecturas({ limit: 5, nodo: "", horas: 0 });
    expect(String(f.mock.calls[0][0])).toBe("https://api.test/api/lecturas?limit=5&horas=0");
  });
  it("ultima devuelve null si no hay lecturas (404)", async () => {
    const f = vi.fn().mockResolvedValue(respuesta({ error: "No hay lecturas" }, 404));
    expect(await crearCliente("https://api.test", f).ultima({ nodo: "x" })).toBeNull();
  });
  it("errores HTTP y de red se convierten en ErrorApi", async () => {
    const f500 = vi.fn().mockResolvedValue(respuesta({}, 503));
    await expect(crearCliente("https://api.test", f500).health()).rejects.toMatchObject({ estado: 503 });
    const fred = vi.fn().mockRejectedValue(new TypeError("fallo"));
    await expect(crearCliente("https://api.test", fred).health()).rejects.toBeInstanceOf(ErrorApi);
  });
});
