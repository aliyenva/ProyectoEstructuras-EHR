// viz/heap.js · Max-Heap como árbol binario y como arreglo
import { get, put, enc } from '../api.js';
import { emit } from '../store.js';
import { $, esc, svg, colorNivel, toast } from '../util.js';

const MAX_VISIBLES = 63;      // 6 niveles
let pacientesSim = '';

export async function refrescarHeap() {
  let h;
  try { h = (await get('/api/estructuras/heap')).heap; }
  catch (e) { $('#heap-svg').innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }

  $('#heap-stats').innerHTML = `
    <span class="stat">Elementos: ${h.tamanio}</span>
    <span class="stat">Intercambios acumulados: ${h.intercambiosTotales}</span>`;

  // Movimientos de la última operación
  const mov = h.movimientos;
  const movIds = new Set();
  const banner = $('#heap-mov');
  if (mov.length) {
    mov.forEach((m) => { movIds.add(m.idDe); movIds.add(m.idA); });
    banner.className = 'banner split';
    banner.textContent = 'Última operación: ' + mov.map((m) =>
      `${m.idDe} (pos ${m.de}) ↔ ${m.idA} (pos ${m.a})`).join(' · ');
  } else {
    banner.className = 'banner';
    banner.textContent = 'La última operación no requirió intercambios en el heap.';
  }

  // Selector del simulador
  const sel = $('#sim-paciente');
  const opciones = h.items.map((i) => `<option value="${esc(i.id)}">${esc(i.id)} · ${esc(i.nombre)} (nivel ${i.nivelTriage})</option>`).join('');
  if (opciones !== pacientesSim) { const v = sel.value; sel.innerHTML = opciones; sel.value = v || (h.items[0] && h.items[0].id) || ''; pacientesSim = opciones; }

  dibujar(h, movIds);

  $('#heap-array').innerHTML = h.items.map((i) => `
    <div class="cell ${movIds.has(i.id) ? 'mov' : ''}" title="${esc(i.nombre)}">
      <b>${i.pos}</b><span style="background:${colorNivel(i.nivelTriage)}">${esc(i.id)}<br>N${i.nivelTriage}</span>
    </div>`).join('');
}

function dibujar(h, movIds) {
  const cont = $('#heap-svg');
  const items = h.items.slice(0, MAX_VISIBLES);
  if (!items.length) { cont.innerHTML = '<div class="empty">La cola está vacía.</div>'; return; }

  const niveles = Math.floor(Math.log2(items.length)) + 1;
  const ancho = Math.max(700, Math.pow(2, niveles - 1) * 64);
  const DY = 80, R = 24;
  const alto = (niveles - 1) * DY + 90;
  cont.innerHTML = '';
  const s = svg('svg', { width: ancho, height: alto, viewBox: `0 0 ${ancho} ${alto}` }, cont);

  const pos = (i) => {
    const nivel = Math.floor(Math.log2(i + 1));
    const indice = i - (Math.pow(2, nivel) - 1);
    const ranuras = Math.pow(2, nivel);
    return { x: ((indice + 0.5) / ranuras) * ancho, y: 40 + nivel * DY };
  };

  items.forEach((it, i) => {
    if (i === 0) return;
    const a = pos(it.padre), b = pos(i);
    svg('line', { x1: a.x, y1: a.y, x2: b.x, y2: b.y, stroke: '#94a3b8', 'stroke-width': 2 }, s);
  });
  items.forEach((it, i) => {
    const p = pos(i);
    const mov = movIds.has(it.id);
    const g = svg('g', { class: 'node' }, s);
    svg('circle', { cx: p.x, cy: p.y, r: R, fill: colorNivel(it.nivelTriage), stroke: mov ? '#0b6bcb' : '#fff', 'stroke-width': mov ? 5 : 2 }, g);
    const t1 = svg('text', { x: p.x, y: p.y - 2, 'text-anchor': 'middle', fill: '#fff', 'font-size': 12, 'font-weight': 700 }, g);
    t1.textContent = it.id.length > 7 ? it.id.slice(0, 6) + '…' : it.id;
    const t2 = svg('text', { x: p.x, y: p.y + 12, 'text-anchor': 'middle', fill: '#fff', 'font-size': 10 }, g);
    t2.textContent = `N${it.nivelTriage} · ${it.sofa}`;
    const t3 = svg('text', { x: p.x, y: p.y - R - 5, 'text-anchor': 'middle', fill: '#64748b', 'font-size': 10 }, g);
    t3.textContent = `[${i}]`;
    svg('title', {}, g).textContent = `${it.nombre}\nNivel ${it.nivelTriage} · SOFA ${it.sofa}`;
  });
  if (h.items.length > MAX_VISIBLES) {
    const t = svg('text', { x: 10, y: alto - 8, fill: '#64748b', 'font-size': 11 }, s);
    t.textContent = `Se muestran los primeros ${MAX_VISIBLES} de ${h.items.length} elementos (el arreglo de abajo muestra todos).`;
  }
}

export function iniciarHeap() {
  $('#sim-aplicar').addEventListener('click', async () => {
    const id = $('#sim-paciente').value;
    const nivel = +$('#sim-nivel').value;
    if (!id) return;
    try {
      const r = await put(`/api/pacientes/${enc(id)}`, { nivelTriage: nivel });
      toast(`${id} → nivel ${nivel}: ${r.trace.heap.intercambios} intercambio(s) en el heap`);
      emit('mutacion', { tipo: 'actualizar', id, trace: r.trace, origen: 'heap' });
      await refrescarHeap();
    } catch (e) { toast(e.message, 'err'); }
  });
}
