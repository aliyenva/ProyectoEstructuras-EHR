// viz/grafo.js · Grafo de derivaciones: visualización + operaciones CRUD en vivo
import { get, post, del, enc } from '../api.js';
import { emit } from '../store.js';
import { $, esc, svg, toast } from '../util.js';

const COLOR = { hospital: '#0b6bcb', centro: '#64748b', especialista: '#0f9d8a' };
const ANCHO = 900, ALTO = 560, R = 30;

let grafo = null;
let seleccion = null;        // {tipo:'nodo'|'arista', id | de,a}
let rutaActual = [];         // ids de la ruta resaltada
let alcanzables = new Set();

function posiciones(nodos) {
  const n = nodos.length, cx = ANCHO / 2, cy = ALTO / 2;
  const rx = ANCHO / 2 - 90, ry = ALTO / 2 - 70;
  const pos = new Map();
  nodos.forEach((nodo, i) => {
    const a = (2 * Math.PI * i) / n - Math.PI / 2;
    pos.set(nodo.id, { x: cx + rx * Math.cos(a), y: cy + ry * Math.sin(a) });
  });
  return pos;
}

function aristaEnRuta(de, a) {
  for (let i = 0; i < rutaActual.length - 1; i++)
    if (rutaActual[i] === de && rutaActual[i + 1] === a) return true;
  return false;
}

function dibujar() {
  const cont = $('#grafo-svg');
  cont.innerHTML = '';
  const s = svg('svg', { width: ANCHO, height: ALTO, viewBox: `0 0 ${ANCHO} ${ALTO}` }, cont);

  const defs = svg('defs', {}, s);
  for (const [id, color] of [['n', '#94a3b8'], ['r', '#f59e0b'], ['s', '#0b6bcb']]) {
    const m = svg('marker', { id: `flecha-${id}`, viewBox: '0 0 10 10', refX: 9, refY: 5, markerWidth: 7, markerHeight: 7, orient: 'auto-start-reverse' }, defs);
    svg('path', { d: 'M 0 0 L 10 5 L 0 10 z', fill: color }, m);
  }

  const pos = posiciones(grafo.nodos);

  // Aristas (curvas para separar A→B de B→A)
  for (const e of grafo.aristas) {
    const a = pos.get(e.de), b = pos.get(e.a);
    if (!a || !b) continue;
    const dx = b.x - a.x, dy = b.y - a.y, d = Math.hypot(dx, dy) || 1;
    const ux = dx / d, uy = dy / d;
    const nx = -uy, ny = ux;
    const curva = 28;
    const x1 = a.x + ux * R, y1 = a.y + uy * R, x2 = b.x - ux * (R + 4), y2 = b.y - uy * (R + 4);
    const cxm = (a.x + b.x) / 2 + nx * curva, cym = (a.y + b.y) / 2 + ny * curva;
    const enRuta = aristaEnRuta(e.de, e.a);
    const sel = seleccion && seleccion.tipo === 'arista' && seleccion.de === e.de && seleccion.a === e.a;
    const color = enRuta ? '#f59e0b' : sel ? '#0b6bcb' : '#94a3b8';
    const marcador = enRuta ? 'r' : sel ? 's' : 'n';
    svg('path', {
      d: `M ${x1} ${y1} Q ${cxm} ${cym} ${x2} ${y2}`, fill: 'none', stroke: color,
      'stroke-width': enRuta ? 4 : 2, 'marker-end': `url(#flecha-${marcador})`,
    }, s);

    // Etiqueta con minutos (clic = seleccionar arista)
    const lx = (x1 + 2 * cxm + x2) / 4, ly = (y1 + 2 * cym + y2) / 4;
    const g = svg('g', { class: 'node' }, s);
    svg('rect', { x: lx - 20, y: ly - 9, width: 40, height: 18, rx: 9, fill: '#fff', stroke: color }, g);
    const t = svg('text', { x: lx, y: ly + 4, 'text-anchor': 'middle', 'font-size': 11, fill: '#334155' }, g);
    t.textContent = `${e.minutos} min`;
    svg('title', {}, g).textContent = `${e.de} → ${e.a}\n${e.minutos} min\n${e.motivo || ''}`;
    g.addEventListener('click', () => { seleccion = { tipo: 'arista', de: e.de, a: e.a, minutos: e.minutos, motivo: e.motivo }; mostrarSeleccion(); dibujar(); });
  }

  // Nodos
  for (const n of grafo.nodos) {
    const p = pos.get(n.id);
    const enRuta = rutaActual.includes(n.id);
    const alc = alcanzables.has(n.id);
    const sel = seleccion && seleccion.tipo === 'nodo' && seleccion.id === n.id;
    const g = svg('g', { class: 'node' }, s);
    svg('circle', {
      cx: p.x, cy: p.y, r: R, fill: COLOR[n.tipo] || '#64748b',
      stroke: enRuta ? '#f59e0b' : sel ? '#1e293b' : alc ? '#1e9e6a' : '#fff',
      'stroke-width': enRuta || sel || alc ? 5 : 2,
    }, g);
    const ic = svg('text', { x: p.x, y: p.y + 5, 'text-anchor': 'middle', fill: '#fff', 'font-size': 16, 'font-weight': 700 }, g);
    ic.textContent = n.tipo === 'hospital' ? 'H' : n.tipo === 'centro' ? 'C' : '+';
    const nom = svg('text', { x: p.x, y: p.y + R + 15, 'text-anchor': 'middle', fill: '#1e293b', 'font-size': 12, 'font-weight': 600 }, g);
    nom.textContent = n.nombre;
    svg('title', {}, g).textContent = `${n.nombre} (${n.id}) · ${n.tipo}`;
    g.addEventListener('click', () => { seleccion = { tipo: 'nodo', id: n.id, nombre: n.nombre }; mostrarSeleccion(); dibujar(); });
  }
}

function mostrarSeleccion() {
  const info = $('#grafo-info');
  if (!seleccion) { info.textContent = 'Haga clic en un nodo o en el tiempo de una arista para seleccionarlo.'; return; }
  if (seleccion.tipo === 'nodo') {
    info.innerHTML = `Nodo <b>${esc(seleccion.nombre)}</b> (${esc(seleccion.id)})
      <button class="btn-secondary" id="g-alcanzables">Alcanzables (BFS)</button>
      <button class="btn-danger" id="g-del-nodo">Eliminar nodo</button>`;
    $('#g-alcanzables').onclick = async () => {
      try {
        const r = await get(`/api/grafo/alcanzables?de=${enc(seleccion.id)}`);
        alcanzables = new Set(r.alcanzables);
        info.innerHTML += `<div class="small">Desde ${esc(seleccion.id)} se puede derivar a: ${esc(r.alcanzables.join(' → '))}</div>`;
        dibujar();
      } catch (e) { toast(e.message, 'err'); }
    };
    $('#g-del-nodo').onclick = async () => {
      if (!confirm(`¿Eliminar el nodo ${seleccion.id} y todas sus derivaciones?`)) return;
      await operar(() => del(`/api/grafo/nodos/${enc(seleccion.id)}`), 'Nodo eliminado');
    };
  } else {
    info.innerHTML = `Derivación <b>${esc(seleccion.de)} → ${esc(seleccion.a)}</b> · ${seleccion.minutos} min ${seleccion.motivo ? '· ' + esc(seleccion.motivo) : ''}
      <button class="btn-danger" id="g-del-arista">Eliminar derivación</button>`;
    $('#g-del-arista').onclick = () => operar(
      () => del(`/api/grafo/aristas?de=${enc(seleccion.de)}&a=${enc(seleccion.a)}`), 'Derivación eliminada');
  }
}

async function operar(fn, mensaje) {
  try {
    const r = await fn();
    toast(mensaje);
    grafo = r.grafo;
    seleccion = null; rutaActual = []; alcanzables = new Set();
    poblarSelectores();
    mostrarSeleccion();
    dibujar();
    listarDerivaciones();
    emit('mutacion', { tipo: 'grafo', origen: 'grafo' });
  } catch (e) { toast(e.message, 'err'); }
}

function poblarSelectores() {
  const opciones = grafo.nodos.map((n) => `<option value="${esc(n.id)}">${esc(n.nombre)} (${esc(n.id)})</option>`).join('');
  for (const id of ['#ga-de', '#ga-a', '#gr-de', '#gr-a', '#gd-de', '#gd-a']) {
    const sel = $(id);
    const previo = sel.value;
    sel.innerHTML = opciones;
    if (previo && grafo.nodos.some((n) => n.id === previo)) sel.value = previo;
  }
  if (grafo.nodos.length > 1) { if ($('#ga-a').value === $('#ga-de').value) $('#ga-a').selectedIndex = 1; }
  $('#grafo-stats').innerHTML = `<span class="stat">Nodos: ${grafo.nodos.length}</span><span class="stat">Aristas: ${grafo.aristas.length}</span><span class="stat">Derivaciones registradas: ${grafo.derivaciones.length}</span>`;
}

function listarDerivaciones() {
  $('#deriv-list').innerHTML = grafo.derivaciones.length
    ? grafo.derivaciones.slice().reverse().map((d) =>
      `<li>#${d.numero} · <b>${esc(d.idPaciente)}</b>: ${esc(d.de)} → ${esc(d.a)}${d.motivo ? ' · ' + esc(d.motivo) : ''}</li>`).join('')
    : '<li class="muted">Sin derivaciones registradas.</li>';
}

async function poblarPacientes() {
  try {
    const r = await get('/api/pacientes');
    const sel = $('#gd-paciente');
    const previo = sel.value;
    sel.innerHTML = r.pacientes.map((p) => `<option value="${esc(p.idPaciente)}">${esc(p.idPaciente)} · ${esc(p.nombre)}</option>`).join('');
    if (previo) sel.value = previo;
  } catch { /* se informa en la vista de gestión */ }
}

export async function refrescarGrafo() {
  try {
    grafo = (await get('/api/grafo')).grafo;
  } catch (e) { $('#grafo-svg').innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
  poblarSelectores();
  dibujar();
  listarDerivaciones();
  poblarPacientes();
}

export function iniciarGrafo() {
  $('#form-nodo').addEventListener('submit', (ev) => {
    ev.preventDefault();
    operar(() => post('/api/grafo/nodos', {
      id: $('#gn-id').value.trim(), nombre: $('#gn-nombre').value.trim(), tipo: $('#gn-tipo').value,
    }), 'Nodo agregado').then(() => { $('#form-nodo').reset(); });
  });

  $('#form-arista').addEventListener('submit', (ev) => {
    ev.preventDefault();
    operar(() => post('/api/grafo/aristas', {
      de: $('#ga-de').value, a: $('#ga-a').value, minutos: +$('#ga-min').value, motivo: $('#ga-motivo').value.trim(),
    }), 'Derivación guardada');
  });

  $('#form-ruta').addEventListener('submit', async (ev) => {
    ev.preventDefault();
    const out = $('#ruta-resultado');
    try {
      const r = await get(`/api/grafo/ruta?de=${enc($('#gr-de').value)}&a=${enc($('#gr-a').value)}`);
      rutaActual = r.camino; alcanzables = new Set();
      out.textContent = `Ruta: ${r.camino.join(' → ')} · ${r.minutos} min en total`;
      dibujar();
    } catch (e) {
      rutaActual = []; out.textContent = e.message; dibujar();
    }
  });

  $('#form-derivar').addEventListener('submit', (ev) => {
    ev.preventDefault();
    operar(() => post('/api/grafo/derivar', {
      idPaciente: $('#gd-paciente').value, de: $('#gd-de').value, a: $('#gd-a').value, motivo: $('#gd-motivo').value.trim(),
    }), 'Paciente derivado');
  });
}
