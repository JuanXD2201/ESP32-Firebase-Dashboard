// logica.js - funciones puras del dashboard (sin DOM): se prueban con Vitest.

export const UMBRAL_DATOS_OBSOLETOS_S = 120; // una lectura más vieja que esto se marca como obsoleta

export function formatearNumero(valor, decimales = 1) {
  return typeof valor === "number" && Number.isFinite(valor) ? valor.toFixed(decimales) : "—";
}

export function formatearFecha(iso) {
  if (!iso) return "—";
  const d = new Date(iso);
  return Number.isNaN(d.getTime())
    ? "—"
    : d.toLocaleString("es-CO", { dateStyle: "short", timeStyle: "medium" });
}

export function antiguedadSegundos(iso, ahora = Date.now()) {
  if (!iso) return null;
  const t = new Date(iso).getTime();
  return Number.isNaN(t) ? null : Math.max(0, Math.round((ahora - t) / 1000));
}

export function textoAntiguedad(segundos) {
  if (segundos === null) return "sin datos";
  if (segundos < 60) return `hace ${segundos} s`;
  if (segundos < 3600) return `hace ${Math.floor(segundos / 60)} min`;
  if (segundos < 86400) return `hace ${Math.floor(segundos / 3600)} h`;
  return `hace ${Math.floor(segundos / 86400)} d`;
}

/**
 * Estado de la conexión (¿responde la API?) separado del estado de los datos
 * (¿qué tan reciente es la última lectura?).
 */
export function estadoSistema({ apiOk, cargando, lectura, ahora = Date.now() }) {
  const edad = lectura ? antiguedadSegundos(lectura.creadoEn, ahora) : null;
  let conexion;
  if (cargando && apiOk === null) conexion = { clase: "cargando", texto: "Cargando…" };
  else if (apiOk === false) conexion = { clase: "error", texto: "API desconectada" };
  else conexion = { clase: "ok", texto: "API conectada" };

  let datos;
  if (!lectura) datos = { clase: "vacio", texto: "Sin lecturas" };
  else if (edad !== null && edad > UMBRAL_DATOS_OBSOLETOS_S)
    datos = { clase: "obsoleto", texto: `Última lectura ${textoAntiguedad(edad)}` };
  else datos = { clase: "ok", texto: `Datos recientes (${textoAntiguedad(edad)})` };
  return { conexion, datos, edad };
}

/** URL pública a mostrar: la real del navegador si no es local; si no, la configurada. */
export function resolverUrlDespliegue(ubicacion, configurada) {
  const host = ubicacion?.hostname ?? "";
  const local =
    host === "" ||
    host === "localhost" ||
    host === "127.0.0.1" ||
    host === "[::1]" ||
    /^(10|192\.168)\./.test(host) ||
    /^172\.(1[6-9]|2\d|3[01])\./.test(host);
  if (!local && ubicacion?.origin) return ubicacion.origin;
  if (configurada && !/localhost|127\.0\.0\.1/.test(configurada)) return configurada;
  return null;
}

/** Datos de demostración: SOLO se usan si el usuario activa el modo demo explícitamente. */
export function generarDemo(cantidad = 40, ahora = Date.now()) {
  const filas = [];
  for (let i = 0; i < cantidad; i++) {
    const t = ahora - (cantidad - 1 - i) * 30000;
    filas.push({
      id: `demo-${i}`,
      nodo: "DEMO",
      seq: i,
      temperatura: 24 + 4 * Math.sin(i / 6),
      humedad: 55 + 10 * Math.cos(i / 8),
      bombillo: 24 + 4 * Math.sin(i / 6) >= 28,
      rssi: -70 + Math.round(5 * Math.sin(i)),
      snr: 8,
      simulado: true,
      creadoEn: new Date(t).toISOString(),
    });
  }
  return filas.reverse(); // más reciente primero, como la API
}

export function escaparHtml(texto) {
  return String(texto ?? "").replace(
    /[&<>"']/g,
    (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c],
  );
}
