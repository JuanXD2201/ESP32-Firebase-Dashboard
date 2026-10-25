// api.js - cliente de la API REST. No contiene secretos: la API solo expone lecturas por GET.

const TIMEOUT_MS = 8000;

export class ErrorApi extends Error {
  constructor(mensaje, estado = null) {
    super(mensaje);
    this.estado = estado;
  }
}

async function pedir(base, ruta, parametros = {}, fetchImpl = fetch) {
  const url = new URL(ruta, base.endsWith("/") ? base : `${base}/`);
  for (const [k, v] of Object.entries(parametros)) {
    if (v !== undefined && v !== null && v !== "") url.searchParams.set(k, v);
  }
  const control = new AbortController();
  const temporizador = setTimeout(() => control.abort(), TIMEOUT_MS);
  try {
    const r = await fetchImpl(url, { signal: control.signal, headers: { Accept: "application/json" } });
    if (r.status === 404) return null;
    if (!r.ok) throw new ErrorApi(`La API respondió ${r.status}`, r.status);
    return await r.json();
  } catch (e) {
    if (e instanceof ErrorApi) throw e;
    throw new ErrorApi(e.name === "AbortError" ? "Tiempo de espera agotado" : "No se pudo conectar con la API");
  } finally {
    clearTimeout(temporizador);
  }
}

export function crearCliente(base, fetchImpl = fetch) {
  const llamar = (ruta, p) => pedir(base, ruta, p, fetchImpl);
  return {
    health: () => llamar("health"),
    nodos: async () => (await llamar("api/nodos")) ?? [],
    lecturas: async ({ limit = 50, nodo, horas } = {}) =>
      (await llamar("api/lecturas", { limit, nodo, horas })) ?? [],
    mensajes: async ({ limit = 20 } = {}) => (await llamar("api/mensajes", { limit })) ?? [],
    ultima: ({ nodo } = {}) => llamar("api/lecturas/ultima", { nodo }), // null si no hay lecturas
  };
}
