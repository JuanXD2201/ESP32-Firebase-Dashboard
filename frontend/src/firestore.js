// firestore.js - cliente de SOLO LECTURA de Firestore por REST (sin SDK ni credenciales).
// Misma interfaz que api.js. Las reglas (firebase/firestore.rules) solo permiten leer `lecturas`;
// las escrituras las hace la API con la cuenta de servicio.

import { ErrorApi } from "./api.js";

const TIMEOUT_MS = 8000;

/** Convierte un valor tipado de la REST de Firestore en un valor JS. */
export function valorFirestore(v) {
  if (v === undefined || v === null) return undefined;
  if ("stringValue" in v) return v.stringValue;
  if ("integerValue" in v) return Number(v.integerValue);
  if ("doubleValue" in v) return v.doubleValue;
  if ("booleanValue" in v) return v.booleanValue;
  if ("timestampValue" in v) return v.timestampValue;
  if ("nullValue" in v) return null;
  return undefined;
}

export function documentoALectura(doc) {
  const lectura = { id: doc.name.split("/").pop() };
  for (const [campo, valor] of Object.entries(doc.fields ?? {})) lectura[campo] = valorFirestore(valor);
  return lectura;
}

export function construirConsulta({ coleccion = "lecturas", limit = 50, nodo, horas, ahora = Date.now() } = {}) {
  const filtros = [];
  if (nodo) filtros.push({ fieldFilter: { field: { fieldPath: "nodo" }, op: "EQUAL", value: { stringValue: nodo } } });
  if (horas) {
    const desde = new Date(ahora - horas * 3600 * 1000).toISOString();
    filtros.push({
      fieldFilter: { field: { fieldPath: "creadoEn" }, op: "GREATER_THAN_OR_EQUAL", value: { timestampValue: desde } },
    });
  }
  const consulta = {
    from: [{ collectionId: coleccion }],
    orderBy: [{ field: { fieldPath: "creadoEn" }, direction: "DESCENDING" }],
    limit: Math.max(1, Math.min(limit, 500)),
  };
  if (filtros.length === 1) consulta.where = filtros[0];
  else if (filtros.length > 1) consulta.where = { compositeFilter: { op: "AND", filters: filtros } };
  return { structuredQuery: consulta };
}

export function crearClienteFirestore(proyecto, fetchImpl = fetch) {
  const base = `https://firestore.googleapis.com/v1/projects/${encodeURIComponent(proyecto)}/databases/(default)/documents`;

  async function consultar(opciones) {
    const control = new AbortController();
    const temporizador = setTimeout(() => control.abort(), TIMEOUT_MS);
    try {
      const r = await fetchImpl(`${base}:runQuery`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(construirConsulta(opciones)),
        signal: control.signal,
      });
      if (!r.ok) {
        const permiso = r.status === 403 ? " (revisa las reglas de Firestore)" : "";
        throw new ErrorApi(`Firestore respondió ${r.status}${permiso}`, r.status);
      }
      const filas = await r.json();
      return filas.filter((f) => f.document).map((f) => documentoALectura(f.document));
    } catch (e) {
      if (e instanceof ErrorApi) throw e;
      throw new ErrorApi(e.name === "AbortError" ? "Tiempo de espera agotado" : "No se pudo conectar con Firestore");
    } finally {
      clearTimeout(temporizador);
    }
  }

  return {
    // Lectura directa: no hay endpoint /health; una consulta de 1 documento hace de comprobación.
    health: async () => {
      await consultar({ limit: 1 });
      return { almacenamiento: `firestore (${proyecto}, lectura directa)`, autenticacion_escritura: true };
    },
    nodos: async () => [...new Set((await consultar({ limit: 200 })).map((l) => l.nodo))],
    lecturas: (opciones = {}) => consultar(opciones),
    mensajes: ({ limit = 20 } = {}) => consultar({ coleccion: "mensajes", limit }),
    ultima: async (opciones = {}) => (await consultar({ ...opciones, limit: 1 }))[0] ?? null,
  };
}
