// util.js · Utilidades compartidas

export const $  = (sel, raiz = document) => raiz.querySelector(sel);
export const $$ = (sel, raiz = document) => [...raiz.querySelectorAll(sel)];

/** Escapa HTML (todo dato del servidor pasa por aquí antes de usar innerHTML) */
export const esc = (s) => String(s ?? '').replace(/[&<>"']/g,
  (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

export function debounce(fn, ms) {
  let t;
  return (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); };
}

export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/** Colores y nombres por nivel de triaje (rojo/ámbar para alertas) */
export const NIVEL = {
  1: { nombre: 'Crítico',        color: '#d93025' },
  2: { nombre: 'Emergencia',     color: '#e8590c' },
  3: { nombre: 'Urgente',        color: '#f59e0b' },
  4: { nombre: 'Menos urgente',  color: '#2b8cd6' },
  5: { nombre: 'No urgente',     color: '#64748b' },
};
export const colorNivel = (n) => (NIVEL[n] || NIVEL[5]).color;
export const badgeNivel = (n) =>
  `<span class="badge" style="background:${colorNivel(n)}">${esc(n)} · ${esc((NIVEL[n] || NIVEL[5]).nombre)}</span>`;

/** Crea un elemento SVG con atributos */
export function svg(tag, attrs = {}, padre) {
  const e = document.createElementNS('http://www.w3.org/2000/svg', tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  if (padre) padre.appendChild(e);
  return e;
}

export function toast(mensaje, tipo = 'ok', ms = 4500) {
  const cont = $('#toasts');
  const t = document.createElement('div');
  t.className = `toast ${tipo === 'ok' ? '' : tipo}`;
  t.textContent = mensaje;
  cont.appendChild(t);
  setTimeout(() => t.remove(), ms);
}

/** Convierte el "trace" que devuelve el backend en chips legibles */
export function chipsTrace(t) {
  if (!t) return '';
  const c = [];
  if (t.hash) {
    c.push(`<span class="tchip${t.hash.split ? ' split' : ''}">Hash: bucket ${t.hash.bucket}`
      + `${t.hash.split ? ' · SPLIT (ahora ' + t.hash.buckets + ' buckets)' : ''}`
      + ` · carga ${t.hash.carga}</span>`);
  }
  if (t.arbol) {
    c.push(`<span class="tchip">Árbol RN: ${t.arbol.tamanioAntes} → ${t.arbol.tamanio} nodos UCI`
      + ` · ${t.arbol.rotaciones} rotación(es)</span>`);
  }
  if (t.heap) {
    c.push(`<span class="tchip">Heap: ${t.heap.intercambios} intercambio(s) · ${t.heap.tamanio} en cola</span>`);
  }
  return c.join('');
}

/** Orden de atención del heap (misma regla que ColaPrioridad.h) */
export function ordenTriaje(a, b) {
  if (a.nivelTriage !== b.nivelTriage) return a.nivelTriage - b.nivelTriage;
  if (a.sofa !== b.sofa) return b.sofa - a.sofa;
  return a.id < b.id ? -1 : a.id > b.id ? 1 : 0;
}
