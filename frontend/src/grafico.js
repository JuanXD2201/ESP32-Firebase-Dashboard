// grafico.js - gráfico SVG de líneas (temperatura y humedad) sin dependencias.

const W = 800;
const H = 240;
const M = { izq: 40, der: 40, arr: 12, aba: 28 };

function escala(valores, margen = 1) {
  const min = Math.min(...valores) - margen;
  const max = Math.max(...valores) + margen;
  return max === min ? [min - 1, max + 1] : [min, max];
}

function polilinea(puntos, color) {
  const d = puntos.map(([x, y]) => `${x.toFixed(1)},${y.toFixed(1)}`).join(" ");
  return `<polyline fill="none" stroke="${color}" stroke-width="2" stroke-linejoin="round" points="${d}"/>`;
}

/** @param lecturas más reciente primero (como la devuelve la API) */
export function dibujarGrafico(contenedor, lecturas) {
  if (!lecturas.length) {
    contenedor.innerHTML = '<p class="vacio-texto">Sin datos para graficar.</p>';
    return;
  }
  const filas = [...lecturas].reverse(); // cronológico
  const tiempos = filas.map((l) => new Date(l.creadoEn).getTime());
  const t0 = Math.min(...tiempos);
  const t1 = Math.max(...tiempos);
  const x = (t, i) =>
    M.izq + (t1 === t0 ? (i / Math.max(filas.length - 1, 1)) : (t - t0) / (t1 - t0)) * (W - M.izq - M.der);

  const [tMin, tMax] = escala(filas.map((l) => l.temperatura));
  const [hMin, hMax] = escala(filas.map((l) => l.humedad), 2);
  const yT = (v) => M.arr + (1 - (v - tMin) / (tMax - tMin)) * (H - M.arr - M.aba);
  const yH = (v) => M.arr + (1 - (v - hMin) / (hMax - hMin)) * (H - M.arr - M.aba);

  const lineasT = filas.map((l, i) => [x(tiempos[i], i), yT(l.temperatura)]);
  const lineasH = filas.map((l, i) => [x(tiempos[i], i), yH(l.humedad)]);
  const hora = (t) => new Date(t).toLocaleTimeString("es-CO", { hour: "2-digit", minute: "2-digit" });

  contenedor.innerHTML = `
    <svg viewBox="0 0 ${W} ${H}" preserveAspectRatio="none" aria-hidden="true">
      <line x1="${M.izq}" y1="${H - M.aba}" x2="${W - M.der}" y2="${H - M.aba}" stroke="currentColor" opacity=".25"/>
      ${polilinea(lineasT, "var(--temp)")}
      ${polilinea(lineasH, "var(--hum)")}
      <g font-size="11" fill="currentColor" opacity=".7">
        <text x="4" y="${yT(tMax) + 10}">${tMax.toFixed(0)}°</text>
        <text x="4" y="${yT(tMin)}">${tMin.toFixed(0)}°</text>
        <text x="${W - 4}" y="${yH(hMax) + 10}" text-anchor="end">${hMax.toFixed(0)}%</text>
        <text x="${W - 4}" y="${yH(hMin)}" text-anchor="end">${hMin.toFixed(0)}%</text>
        <text x="${M.izq}" y="${H - 8}">${hora(t0)}</text>
        <text x="${W - M.der}" y="${H - 8}" text-anchor="end">${hora(t1)}</text>
      </g>
    </svg>`;
}
