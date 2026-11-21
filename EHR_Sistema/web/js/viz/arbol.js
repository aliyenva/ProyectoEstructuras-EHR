// viz/arbol.js · Árbol Rojo-Negro (SVG) con recorridos y detección de cambios
import { get } from '../api.js';
import { $, $$, esc, svg, sleep, colorNivel } from '../util.js';

const DX = 78, DY = 92, R = 25, MARGEN = 40;
let datos = null;
let previo = new Map();      // id -> {color, padre}
let animacion = 0;           // token para cancelar recorridos en curso

/** Asigna coordenadas: x por orden inorden, y por profundidad */
function disponer(raiz) {
  const nodos = [];
  let col = 0;
  (function visitar(n, prof, padre) {
    if (!n) return;
    visitar(n.izq, prof + 1, n);
    n.x = MARGEN + col++ * DX + DX / 2;
    n.y = MARGEN + prof * DY;
    n.padreId = padre ? padre.id : null;
    nodos.push(n);
    visitar(n.der, prof + 1, n);
  })(raiz, 0, null);
  return nodos;
}

function dibujar() {
  const cont = $('#arbol-svg');
  const t = datos;
  $('#arbol-stats').innerHTML = `
    <span class="stat">Nodos: ${t.tamanio}</span>
    <span class="stat">Altura: ${t.altura}</span>
    <span class="stat">Altura negra: ${t.alturaNegra}</span>
    <span class="stat">Rotaciones acumuladas: ${t.rotaciones}</span>`;

  if (!t.raiz) {
    cont.innerHTML = '<div class="empty">No hay pacientes en UCI. Registre un paciente con "Ingresa a UCI" para verlo aquí.</div>';
    previo = new Map();
    return;
  }

  const nodos = disponer(t.raiz);
  const ancho = Math.max(560, nodos.length * DX + MARGEN * 2);
  const alto = (t.altura - 1) * DY + MARGEN * 2 + 30;
  cont.innerHTML = '';
  const s = svg('svg', { width: ancho, height: alto, viewBox: `0 0 ${ancho} ${alto}` }, cont);

  // Aristas
  const porId = new Map(nodos.map((n) => [n.id, n]));
  for (const n of nodos) {
    for (const h of [n.izq, n.der]) {
      if (!h) continue;
      const hh = porId.get(h.id);
      svg('line', { x1: n.x, y1: n.y, x2: hh.x, y2: hh.y, stroke: '#94a3b8', 'stroke-width': 2 }, s);
    }
  }

  // Nodos
  const primera = previo.size === 0;
  for (const n of nodos) {
    const antes = previo.get(n.id);
    const nuevo = !primera && !antes;
    const recoloreado = antes && antes.color !== n.color;
    const rotado = antes && antes.padre !== n.padreId;

    const g = svg('g', { class: 'node' + (nuevo ? ' pulse-new' : ''), 'data-id': n.id, style: 'transform-box:fill-box;transform-origin:center' }, s);
    svg('circle', {
      cx: n.x, cy: n.y, r: R,
      fill: n.color === 'R' ? '#d93025' : '#1f2937',
      stroke: (recoloreado || rotado || nuevo) ? '#f59e0b' : '#fff',
      'stroke-width': (recoloreado || rotado || nuevo) ? 5 : 2,
    }, g);
    const t1 = svg('text', { x: n.x, y: n.y - 2, 'text-anchor': 'middle', fill: '#fff', 'font-size': 12, 'font-weight': 700 }, g);
    t1.textContent = n.id.length > 7 ? n.id.slice(0, 6) + '…' : n.id;
    const t2 = svg('text', { x: n.x, y: n.y + 12, 'text-anchor': 'middle', fill: '#e2e8f0', 'font-size': 10 }, g);
    t2.textContent = `SOFA ${n.sofa}`;
    // Altura / altura negra / balance bajo el nodo
    const t3 = svg('text', { x: n.x, y: n.y + R + 14, 'text-anchor': 'middle', fill: '#64748b', 'font-size': 10 }, g);
    t3.textContent = `h=${n.altura} bh=${n.alturaNegra} fb=${n.balance > 0 ? '+' : ''}${n.balance}`;
    // Barra de color de triaje
    svg('rect', { x: n.x - 10, y: n.y - R - 8, width: 20, height: 4, rx: 2, fill: colorNivel(n.nivelTriage) }, g);
    const tt = svg('title', {}, g);
    tt.textContent = `${n.nombre}\nSOFA ${n.sofa} · triaje ${n.nivelTriage}\n${n.color === 'R' ? 'Rojo' : 'Negro'} · altura ${n.altura} · altura negra ${n.alturaNegra} · balance ${n.balance}`;

    g.addEventListener('click', () => {
      $('#arbol-info').innerHTML = `<b>${esc(n.nombre)}</b> (${esc(n.id)}) · SOFA ${esc(n.sofa)} · triaje ${n.nivelTriage}
        · color <b style="color:${n.color === 'R' ? '#d93025' : '#1f2937'}">${n.color === 'R' ? 'ROJO' : 'NEGRO'}</b>
        · altura ${n.altura} · altura negra ${n.alturaNegra} · factor de balance (altura izq − der) ${n.balance}`;
    });
  }
  previo = new Map(nodos.map((n) => [n.id, { color: n.color, padre: n.padreId }]));
}

/** Resalta nodos en el orden del recorrido, uno por uno */
async function recorrer(tipo) {
  if (!datos || !datos.raiz) return;
  const token = ++animacion;
  $$('[data-recorrido]').forEach((b) => b.classList.toggle('on', b.dataset.recorrido === tipo));
  const orden = datos[tipo];
  const caja = $('#arbol-recorrido');
  caja.innerHTML = `<b>${tipo[0].toUpperCase() + tipo.slice(1)}:</b> ` + orden.map((id, i) => `<span class="rchip" data-i="${i}">${esc(id)}</span>`).join('');
  for (let i = 0; i < orden.length; i++) {
    if (token !== animacion) return;
    $$('#arbol-svg .node circle').forEach((c) => c.setAttribute('stroke', '#fff'));
    const g = $(`#arbol-svg .node[data-id="${CSS.escape(orden[i])}"] circle`);
    if (g) { g.setAttribute('stroke', '#f59e0b'); g.setAttribute('stroke-width', 6); }
    $$('#arbol-recorrido .rchip').forEach((c, j) => c.classList.toggle('on', j === i));
    await sleep(650);
  }
  if (token === animacion) $$('#arbol-svg .node circle').forEach((c) => { c.setAttribute('stroke', '#fff'); c.setAttribute('stroke-width', 2); });
}

export async function refrescarArbol() {
  try {
    datos = (await get('/api/estructuras/arbol')).arbol;
    animacion++;                              // cancela recorridos pendientes
    $$('[data-recorrido]').forEach((b) => b.classList.remove('on'));
    $('#arbol-recorrido').innerHTML = '';
    dibujar();
  } catch (e) {
    $('#arbol-svg').innerHTML = `<div class="empty">${esc(e.message)}</div>`;
  }
}

export function iniciarArbol() {
  $$('[data-recorrido]').forEach((b) => b.addEventListener('click', () => recorrer(b.dataset.recorrido)));
}
