import { crearCliente } from "./api.js";
import { crearClienteFirestore } from "./firestore.js";
import { dibujarGrafico } from "./grafico.js";
import {
  antiguedadSegundos,
  escaparHtml,
  estadoSistema,
  formatearFecha,
  formatearNumero,
  generarDemo,
  resolverUrlDespliegue,
  textoAntiguedad,
} from "./logica.js";

const $ = (id) => document.getElementById(id);

const estado = {
  config: { apiUrl: "", urlDespliegue: "", intervaloActualizacionS: 10 },
  cliente: null,
  demo: new URLSearchParams(location.search).get("demo") === "1",
  apiOk: null,
  cargando: true,
  lecturas: [],
  mensajes: [],
  health: null,
  ultimaConsulta: null,
  temporizador: null,
};

async function cargarConfig() {
  try {
    const r = await fetch("config.json", { cache: "no-store" });
    if (r.ok) Object.assign(estado.config, await r.json());
  } catch {
    /* sin config.json: se queda con los valores por defecto */
  }
  // apiUrl explícita tiene prioridad; si no, lectura directa de Firestore (solo lectura, ver reglas).
  if (estado.config.apiUrl) estado.cliente = crearCliente(estado.config.apiUrl);
  else if (estado.config.firestoreProjectId) {
    estado.cliente = crearClienteFirestore(estado.config.firestoreProjectId);
  }
}

function descripcionFuente() {
  if (estado.config.apiUrl) return estado.config.apiUrl;
  return `Firestore · ${estado.config.firestoreProjectId}`;
}

function filtros() {
  return {
    nodo: $("filtro-nodo").value,
    horas: Number($("filtro-horas").value),
    limit: Number($("filtro-limite").value),
  };
}

async function actualizarNodos() {
  const select = $("filtro-nodo");
  const actual = select.value;
  const nodos = estado.demo ? ["DEMO"] : await estado.cliente.nodos();
  select.innerHTML =
    '<option value="">Todos</option>' +
    nodos.map((n) => `<option value="${escaparHtml(n)}">${escaparHtml(n)}</option>`).join("");
  if (nodos.includes(actual)) select.value = actual;
}

async function consultar() {
  estado.cargando = true;
  pintarEstado();
  const f = filtros();
  try {
    if (estado.demo) {
      estado.lecturas = generarDemo(f.limit);
      estado.apiOk = true;
      estado.health = { almacenamiento: "demostración (sin base de datos)", autenticacion_escritura: false };
    } else if (estado.cliente) {
      const [lecturas, health, mensajes] = await Promise.all([
        estado.cliente.lecturas(f),
        estado.cliente.health(),
        estado.cliente.mensajes({ limit: 20 }).catch(() => estado.mensajes), // un fallo aquí no tumba lo demás
        actualizarNodos().catch(() => {}),
      ]);
      estado.lecturas = lecturas;
      estado.mensajes = mensajes;
      estado.health = health;
      estado.apiOk = true;
      $("aviso-error").hidden = true;
    }
  } catch (e) {
    estado.apiOk = false; // se conservan las últimas lecturas, marcadas como no actualizadas
    $("aviso-error").textContent = `No se pudo actualizar: ${e.message}. Se muestran las últimas lecturas recibidas.`;
    $("aviso-error").hidden = false;
  } finally {
    estado.cargando = false;
    estado.ultimaConsulta = new Date();
    pintar();
  }
}

function pintarEstado() {
  const s = estadoSistema({ apiOk: estado.apiOk, cargando: estado.cargando, lectura: estado.lecturas[0] });
  $("pill-conexion").className = `pill ${s.conexion.clase}`;
  $("pill-conexion").textContent = s.conexion.texto;
  $("pill-datos").className = `pill ${s.datos.clase}`;
  $("pill-datos").textContent = s.datos.texto;
}

function pintar() {
  const u = estado.lecturas[0];
  $("v-temp").textContent = formatearNumero(u?.temperatura);
  $("v-hum").textContent = formatearNumero(u?.humedad);
  const b = $("v-bombillo");
  b.textContent = u ? (u.bombillo ? "ENCENDIDO" : "APAGADO") : "—";
  b.className = `insignia ${u ? (u.bombillo ? "on" : "off") : ""}`;
  $("v-nodo").textContent = u?.nodo ?? "—";
  $("v-seq").textContent = u?.seq ?? "—";
  $("v-fecha").textContent = formatearFecha(u?.creadoEn);
  $("v-rssi").textContent = u?.rssi ?? "—";
  $("v-snr").textContent = formatearNumero(u?.snr);
  $("v-simulado").hidden = !u?.simulado || estado.demo;
  $("v-edad").textContent = pintarEdad(u);

  $("aviso-demo").hidden = !estado.demo;
  $("aviso-config").hidden = estado.demo || Boolean(estado.cliente);
  $("btn-demo").setAttribute("aria-pressed", String(estado.demo));

  $("tabla").innerHTML = estado.lecturas
    .map(
      (l) => `<tr class="${l.simulado ? "simulada" : ""}">
        <td>${formatearFecha(l.creadoEn)}</td><td>${escaparHtml(l.nodo)}</td><td>${l.seq}</td>
        <td>${formatearNumero(l.temperatura)}</td><td>${formatearNumero(l.humedad)}</td>
        <td>${l.bombillo ? "ON" : "OFF"}</td><td>${l.rssi ?? "—"}</td><td>${formatearNumero(l.snr)}</td></tr>`,
    )
    .join("");
  $("vacio").hidden = estado.lecturas.length > 0 || estado.cargando;
  $("contador").textContent = estado.lecturas.length ? `(${estado.lecturas.length})` : "";
  dibujarGrafico($("grafico"), estado.lecturas);

  // textContent: el texto del teléfono nunca se interpreta como HTML.
  $("mensajes").replaceChildren(
    ...estado.mensajes.map((m) => {
      const li = document.createElement("li");
      const hora = document.createElement("time");
      hora.textContent = `${formatearFecha(m.creadoEn)} · ${m.nodo ?? ""}`;
      li.append(hora, document.createTextNode(m.texto ?? ""));
      return li;
    }),
  );
  $("vacio-msg").hidden = estado.mensajes.length > 0 || estado.cargando;
  $("contador-msg").textContent = estado.mensajes.length ? `(${estado.mensajes.length})` : "";

  $("s-api").textContent = estado.demo
    ? "Modo demostración"
    : estado.cliente
      ? `${descripcionFuente()} — ${estado.apiOk ? "responde" : estado.apiOk === false ? "sin respuesta" : "…"}`
      : "No configurada";
  $("s-almacen").textContent = estado.health?.almacenamiento ?? "—";
  $("s-auth").textContent = estado.health ? (estado.health.autenticacion_escritura ? "sí (X-API-Key)" : "NO: la API acepta escrituras sin clave") : "—";
  $("s-consulta").textContent = estado.ultimaConsulta ? estado.ultimaConsulta.toLocaleTimeString("es-CO") : "—";
  $("s-intervalo").textContent = `cada ${estado.config.intervaloActualizacionS} s`;
  pintarEstado();
}

function pintarEdad(u) {
  return u ? `(${textoAntiguedad(antiguedadSegundos(u.creadoEn))})` : "";
}

function pintarDespliegue() {
  const url = resolverUrlDespliegue(location, estado.config.urlDespliegue);
  const caja = $("despliegue-contenido");
  caja.textContent = "";
  if (!url) {
    caja.textContent = "Aplicación pendiente de despliegue";
    $("pie-url").textContent = "";
    return;
  }
  const enlace = document.createElement("a");
  enlace.href = url;
  enlace.textContent = url;
  enlace.target = "_blank";
  enlace.rel = "noopener";
  caja.append(enlace);
  if (navigator.clipboard?.writeText) {
    const copiar = document.createElement("button");
    copiar.type = "button";
    copiar.textContent = "Copiar";
    copiar.addEventListener("click", async () => {
      try {
        await navigator.clipboard.writeText(url);
        copiar.textContent = "¡Copiada!";
      } catch {
        copiar.textContent = "No se pudo copiar";
      }
      setTimeout(() => (copiar.textContent = "Copiar"), 2000);
    });
    caja.append(copiar);
  }
  const pie = document.createElement("a");
  pie.href = url;
  pie.textContent = url;
  $("pie-url").replaceChildren("Aplicación web: ", pie);
}

function programar() {
  clearInterval(estado.temporizador);
  const ms = Math.max(3, Number(estado.config.intervaloActualizacionS) || 10) * 1000;
  estado.temporizador = setInterval(() => {
    if (!document.hidden && (estado.demo || estado.cliente)) consultar();
  }, ms);
}

async function iniciar() {
  await cargarConfig();
  pintarDespliegue();
  for (const id of ["filtro-nodo", "filtro-horas", "filtro-limite"]) $(id).addEventListener("change", consultar);
  $("btn-actualizar").addEventListener("click", consultar);
  $("btn-demo").addEventListener("click", () => {
    estado.demo = !estado.demo;
    estado.apiOk = null;
    estado.lecturas = [];
    consultar();
  });
  setInterval(pintarEstado, 1000); // refresca "hace X s" sin consultar
  programar();
  if (estado.demo || estado.cliente) await consultar();
  else {
    estado.cargando = false;
    pintar();
  }
}

iniciar();
