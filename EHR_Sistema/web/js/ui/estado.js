// ui/estado.js · Indicadores de conexión (backend C++ y MongoDB) + bitácora
import { get, post } from '../api.js';
import { state, on, emit } from '../store.js';
import { $, esc, toast } from '../util.js';

let ultimoEvento = 0;
let bucle = null;

function pintarChips() {
  const cb = $('#chip-backend');
  const cm = $('#chip-mongo');
  const e = state.estado;

  if (state.backend === false || !e) {
    cb.className = 'chip bad';
    cb.querySelector('span').textContent = state.backend === null ? 'Backend: conectando…' : 'Backend C++ desconectado';
    cm.className = 'chip';
    cm.querySelector('span').textContent = 'MongoDB: sin datos';
    return;
  }
  cb.className = 'chip ok';
  cb.querySelector('span').textContent = `Backend C++ en línea · ${e.pacientes} pacientes`;

  if (!e.nube) {
    cm.className = 'chip bad';
    cm.querySelector('span').textContent = 'MongoDB sin conexión (modo local)';
  } else if (!e.ultimoSyncOk) {
    cm.className = 'chip warn';
    cm.querySelector('span').textContent = 'MongoDB: error de sincronización';
  } else {
    cm.className = 'chip ok';
    cm.querySelector('span').textContent = 'MongoDB sincronizado';
  }
  $('#cnt-triaje').textContent = e.triage;
}

async function refrescarEstado() {
  try {
    state.estado = await get('/api/estado');
  } catch { /* api() ya marcó backend offline */ }
  pintarChips();
}

function lineaEvento(ev) {
  const clase = !ev.ok ? 'l-err' : ev.tipo === 'mongo' ? 'l-mongo' : ev.tipo === 'sistema' ? 'l-sistema' : 'l-api';
  const etiqueta = ev.tipo === 'mongo' ? 'MONGO' : ev.tipo === 'sistema' ? 'SISTEMA' : 'API  ';
  const cuerpo = ev.tipo === 'sistema'
    ? esc(ev.mensaje)
    : `${esc(ev.metodo)} ${esc(ev.ruta)} → ${ev.codigo || 'sin respuesta'} (${ev.ms} ms)${ev.mensaje ? ' · ' + esc(ev.mensaje) : ''}`;
  return `<div class="${clase}">${esc(ev.hora)} [${etiqueta}] ${cuerpo}</div>`;
}

async function refrescarEventos() {
  let datos;
  try { datos = await get(`/api/eventos?desde=${ultimoEvento}`); } catch { return; }
  if (!datos.eventos.length) { ultimoEvento = datos.ultimo; return; }

  const cuerpo = $('#log-body');
  const abajo = cuerpo.scrollTop + cuerpo.clientHeight >= cuerpo.scrollHeight - 20;
  cuerpo.insertAdjacentHTML('beforeend', datos.eventos.map(lineaEvento).join(''));
  while (cuerpo.children.length > 300) cuerpo.removeChild(cuerpo.firstChild);
  if (abajo) cuerpo.scrollTop = cuerpo.scrollHeight;

  const ult = datos.eventos[datos.eventos.length - 1];
  $('#ticker').textContent = ult.tipo === 'sistema'
    ? `${ult.hora}  ${ult.mensaje}`
    : `${ult.hora}  ${ult.tipo === 'mongo' ? '☁ MongoDB' : '⇄ API'}  ${ult.metodo} ${ult.ruta} → ${ult.codigo || 'sin respuesta'} (${ult.ms} ms)`
      + (ult.mensaje ? `  ·  ${ult.mensaje}` : '');

  // Pulso en el indicador de Mongo cuando hubo sincronización
  if (datos.eventos.some((e) => e.tipo === 'mongo')) {
    const c = $('#chip-mongo');
    c.classList.remove('pulse'); void c.offsetWidth; c.classList.add('pulse');
    refrescarEstado();
  }
  ultimoEvento = datos.ultimo;
}

export function iniciarEstado() {
  on('backend', pintarChips);
  on('mutacion', () => { refrescarEventos(); refrescarEstado(); });

  $('#btn-log').addEventListener('click', () => $('#log-panel').classList.toggle('hidden'));
  $('#btn-log-cerrar').addEventListener('click', () => $('#log-panel').classList.add('hidden'));

  $('#btn-reconectar').addEventListener('click', async () => {
    toast('Verificando conexión con MongoDB…', 'warn', 2000);
    try {
      const r = await post('/api/nube/reconectar');
      toast(r.nube ? 'Conectado a MongoDB' : 'MongoDB sigue sin responder', r.nube ? 'ok' : 'err');
    } catch (e) { toast(e.message, 'err'); }
    await refrescarEstado();
    await refrescarEventos();
  });

  refrescarEstado();
  refrescarEventos();
  bucle = setInterval(() => { refrescarEstado(); refrescarEventos(); }, 2500);
}

export { refrescarEstado };
