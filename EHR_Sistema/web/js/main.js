// main.js · Arranque de la aplicación, navegación y refresco de visualizadores
import { state, on, emit } from './store.js';
import { $, $$ } from './util.js';
import { iniciarEstado } from './ui/estado.js';
import { iniciarGestion } from './ui/gestion.js';
import { iniciarTriaje } from './ui/triaje.js';
import { iniciarArbol, refrescarArbol } from './viz/arbol.js';
import { refrescarHash } from './viz/hash.js';
import { iniciarHeap, refrescarHeap } from './viz/heap.js';
import { iniciarGrafo, refrescarGrafo } from './viz/grafo.js';

const REFRESCO = {
  arbol: refrescarArbol,
  hash: refrescarHash,
  heap: refrescarHeap,
  grafo: refrescarGrafo,
};

/** Refresca solo el visualizador visible (los demás se actualizan al abrirlos) */
function refrescarVisible() {
  if (state.tabActual === 'estructuras') REFRESCO[state.subActual]();
}

function mostrarTab(nombre) {
  state.tabActual = nombre;
  $$('.tab-btn').forEach((b) => b.classList.toggle('active', b.dataset.tab === nombre));
  $$('.tab').forEach((t) => t.classList.toggle('active', t.id === `tab-${nombre}`));
  emit('tab', nombre);
  refrescarVisible();
}

function mostrarSub(nombre) {
  state.subActual = nombre;
  $$('.sub-btn').forEach((b) => b.classList.toggle('active', b.dataset.sub === nombre));
  $$('.sub').forEach((s) => s.classList.toggle('active', s.id === `sub-${nombre}`));
  refrescarVisible();
}

$$('.tab-btn').forEach((b) => b.addEventListener('click', () => mostrarTab(b.dataset.tab)));
$$('.sub-btn').forEach((b) => b.addEventListener('click', () => mostrarSub(b.dataset.sub)));

// Cada operación que modifica estructuras actualiza el visualizador visible
on('mutacion', refrescarVisible);

// Enlace directo: #triaje, #estructuras/hash, ...
(function leerHash() {
  const [tab, sub] = location.hash.replace('#', '').split('/');
  if (tab && document.getElementById(`tab-${tab}`)) {
    if (sub && REFRESCO[sub]) mostrarSub(sub);
    mostrarTab(tab);
  }
})();

iniciarEstado();
iniciarGestion();
iniciarTriaje();
iniciarArbol();
iniciarHeap();
iniciarGrafo();
