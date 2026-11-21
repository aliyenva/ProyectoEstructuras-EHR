// store.js · Estado global mínimo y bus de eventos (pub/sub)
//
// Eventos:
//   'backend'   (bool)           cambió la conexión con el backend C++
//   'mutacion'  ({tipo, id, trace})  se modificó una estructura (alta/baja/cambio/despacho/grafo)
//   'tab'       (nombre)         se mostró una pestaña principal o subpestaña

const oyentes = {};

export function on(evento, fn) {
  (oyentes[evento] ||= []).push(fn);
}

export function emit(evento, datos) {
  for (const fn of oyentes[evento] || []) {
    try { fn(datos); } catch (e) { console.error(`Error en oyente de "${evento}"`, e); }
  }
}

export const state = {
  backend: null,        // true / false / null (desconocido)
  estado: null,         // última respuesta de /api/estado
  tabActual: 'gestion',
  subActual: 'arbol',
};

export function setBackend(ok) {
  if (state.backend !== ok) {
    state.backend = ok;
    emit('backend', ok);
  }
}
