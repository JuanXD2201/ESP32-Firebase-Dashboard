import { describe, expect, it, vi } from "vitest";
import { crearClienteFirestore, construirConsulta, documentoALectura } from "../src/firestore.js";

const DOC = {
  name: "projects/p/databases/(default)/documents/lecturas/emisor-01_3_42",
  fields: {
    nodo: { stringValue: "emisor-01" },
    seq: { integerValue: "42" },
    temperatura: { doubleValue: 23.4 },
    humedad: { doubleValue: 58.2 },
    bombillo: { booleanValue: true },
    rssi: { integerValue: "-65" },
    creadoEn: { timestampValue: "2026-10-25T20:00:00Z" },
  },
};

describe("firestore REST", () => {
  it("convierte un documento tipado", () => {
    expect(documentoALectura(DOC)).toEqual({
      id: "emisor-01_3_42", nodo: "emisor-01", seq: 42, temperatura: 23.4, humedad: 58.2,
      bombillo: true, rssi: -65, creadoEn: "2026-10-25T20:00:00Z",
    });
  });

  it("construye filtros por nodo y por tiempo", () => {
    const ahora = Date.parse("2026-10-25T12:00:00Z");
    const q = construirConsulta({ nodo: "a", horas: 1, limit: 10, ahora }).structuredQuery;
    expect(q.limit).toBe(10);
    expect(q.orderBy[0].direction).toBe("DESCENDING");
    expect(q.where.compositeFilter.filters).toHaveLength(2);
    expect(q.where.compositeFilter.filters[1].fieldFilter.value.timestampValue).toBe("2026-10-25T11:00:00.000Z");
    expect(construirConsulta({}).structuredQuery.where).toBeUndefined();
    expect(construirConsulta({ limit: 9999 }).structuredQuery.limit).toBe(500);
  });

  it("lecturas ignora filas sin documento y ultima devuelve null si no hay", async () => {
    const f = vi.fn().mockResolvedValue({ ok: true, json: async () => [{ readTime: "x" }] });
    const c = crearClienteFirestore("p", f);
    expect(await c.lecturas()).toEqual([]);
    expect(await c.ultima()).toBeNull();
    expect(f.mock.calls[0][0]).toContain("/projects/p/databases/(default)/documents:runQuery");
  });

  it("errores 403 y de red se informan", async () => {
    const f403 = vi.fn().mockResolvedValue({ ok: false, status: 403, json: async () => ({}) });
    await expect(crearClienteFirestore("p", f403).lecturas()).rejects.toMatchObject({ estado: 403 });
    const fred = vi.fn().mockRejectedValue(new TypeError("x"));
    await expect(crearClienteFirestore("p", fred).health()).rejects.toThrow(/Firestore/);
  });
});
