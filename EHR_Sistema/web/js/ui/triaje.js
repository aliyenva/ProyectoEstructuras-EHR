// ui/triaje.js · Cola de urgencias conectada al Max-Heap del backend
import { get, post, put, enc } from '../api.js';
import { emit, on, state } from '../store.js';
import { $, esc, toast, badgeNivel, colorNivel, sleep, ordenTriaje } from '../util.js';

let despachando = false;

function tarjetaSiguiente(p) {
  const caja = $('#siguiente');
  if (!p) {
    caja.innerHTML = '<div class="next vacio">La cola de urgencias está vacía.</div>';
    $('#btn-despachar').disabled = true;
    return;
  }
  $('#btn-despachar').disabled = despachando;
  caja.innerHTML = `
    <div class="next" style="border-left-color:${colorNivel(p.nivelTriage)}">
      <h3>${esc(p.nombre)} <small>(${esc(p.idPaciente)})</small></h3>
      <div class="meta">${badgeNivel(p.nivelTriage)} ${p.enUCI ? '<b class="uci-si">UCI</b>' : ''}
        · ${p.edad ? esc(p.edad) + ' años · ' : ''}Sangre ${esc(p.tipoSangre)}</div>
      <div class="sofa-big">SOFA <b>${esc(p.scoreSOFA)}</b> ${p.sofaMedido ? '(calculado)' : '(manual)'}
        ${p.diagnostico ? '· ' + esc(p.diagnostico) : ''}</div>
      ${(p.alergias || []).length ? `<div class="small" style="margin-top:4px">⚠ Alergias: ${esc(p.alergias.join(', '))}</div>` : ''}
    </div>`;
}

async function cargar() {
  let r;
  try { r = await get('/api/triage'); } catch (e) {
    $('#tabla-triaje tbody').innerHTML = `<tr><td colspan="7" class="empty">${esc(e.message)}</td></tr>`;
    return;
  }
  const items = r.cola.items.slice();
  tarjetaSiguiente(r.siguiente);
  $('#triage-info').textContent = `${items.length} paciente(s) en el heap · ${r.cola.intercambiosTotales} intercambios acumulados`;

  const ordenados = items.slice().sort(ordenTriaje);
  const tb = $('#tabla-triaje tbody');
  if (!ordenados.length) {
    tb.innerHTML = '<tr><td colspan="7" class="empty">Sin pacientes en espera</td></tr>';
    return;
  }
  tb.innerHTML = ordenados.map((p, i) => `
    <tr>
      <td><b>${i + 1}</b></td>
      <td>${p.pos}</td>
      <td>${esc(p.id)}</td>
      <td>${esc(p.nombre)}</td>
      <td>
        <select class="nivel" data-id="${esc(p.id)}" style="color:${colorNivel(p.nivelTriage)}">
          ${[1, 2, 3, 4, 5].map((n) => `<option value="${n}" ${n === p.nivelTriage ? 'selected' : ''}>${n}</option>`).join('')}
        </select>
      </td>
      <td>${esc(p.sofa)}</td>
      <td>${p.enUCI ? '<span class="uci-si">UCI</span>' : '–'}</td>
    </tr>`).join('');
}

// ── Flujo asincrónico del despacho ───────────────────────────────
function pasoFlujo(lista, texto, clase) {
  const li = document.createElement('li');
  li.className = clase;
  li.innerHTML = `<span class="ic"></span><span>${texto}</span>`;
  lista.appendChild(li);
  return li;
}

async function despachar() {
  if (despachando) return;
  despachando = true;
  const btn = $('#btn-despachar');
  btn.disabled = true;
  const flujo = $('#flow');
  flujo.innerHTML = '';

  const p1 = pasoFlujo(flujo, 'Enviando <code>POST /api/triage/despachar</code> al backend C++…', 'run');
  try {
    const r = await post('/api/triage/despachar');
    p1.className = 'done';
    const t = r.trace;
    await sleep(350);
    pasoFlujo(flujo, `Max-Heap: extrae la raíz <b>${esc(r.paciente.nombre)}</b> (nivel ${r.paciente.nivelTriage}, SOFA ${esc(r.paciente.scoreSOFA)}) · ${t.heap.intercambios} intercambio(s) para reorganizar`, 'done');
    await sleep(350);
    pasoFlujo(flujo, `Tabla Hash: elimina del bucket ${t.hash.bucket} (${t.hash.elementos} registros restantes, carga ${t.hash.carga})`, 'done');
    await sleep(350);
    pasoFlujo(flujo, r.paciente.enUCI
      ? `Árbol Rojo-Negro: elimina por clave (SOFA ${esc(r.paciente.scoreSOFA)}, ${esc(r.paciente.idPaciente)}) · ${t.arbol.tamanioAntes} → ${t.arbol.tamanio} nodos · ${t.arbol.rotaciones} rotación(es)`
      : 'Árbol Rojo-Negro: sin cambios (el paciente no estaba en UCI)', 'done');
    await sleep(350);
    const pm = pasoFlujo(flujo, 'MongoDB: sincronizando <code>DELETE</code> en la nube…', 'run');
    // La llamada a Mongo ya ocurrió dentro del backend; se consulta la bitácora para confirmarla
    await sleep(500);
    try {
      const ev = await get('/api/eventos?desde=0');
      const m = ev.eventos.filter((e) => e.tipo === 'mongo' && e.metodo === 'DELETE').pop();
      if (m && m.ok) { pm.className = 'done'; pm.lastChild.innerHTML = `MongoDB: DELETE sincronizado (HTTP ${m.codigo}, ${m.ms} ms)`; }
      else if (m) { pm.className = 'err'; pm.lastChild.textContent = 'MongoDB: la sincronización falló (el dato local sí se eliminó)'; }
      else { pm.className = 'done'; pm.lastChild.textContent = 'MongoDB: modo local, sin sincronización'; }
    } catch { pm.className = 'done'; }
    toast(`Atendido: ${r.paciente.nombre} (nivel ${r.paciente.nivelTriage})`);
    emit('mutacion', { tipo: 'despachar', id: r.paciente.idPaciente, trace: r.trace, origen: 'triaje' });
  } catch (e) {
    p1.className = 'err';
    p1.lastChild.textContent = e.message;
    toast(e.message, 'err');
  } finally {
    despachando = false;
    await cargar();
  }
}

async function cambiarNivel(id, nivel) {
  try {
    const r = await put(`/api/pacientes/${enc(id)}`, { nivelTriage: nivel });
    const mov = r.trace.heap.movimientos;
    toast(`${id}: nivel ${nivel} · el heap hizo ${r.trace.heap.intercambios} intercambio(s)`
      + (mov.length ? ` (pos ${mov[0].de} → ${mov[mov.length - 1].a})` : ''));
    emit('mutacion', { tipo: 'actualizar', id, trace: r.trace, origen: 'triaje' });
  } catch (e) { toast(e.message, 'err'); }
  await cargar();
}

export function iniciarTriaje() {
  $('#btn-despachar').addEventListener('click', despachar);
  $('#tabla-triaje').addEventListener('change', (ev) => {
    const s = ev.target.closest('select.nivel');
    if (s) cambiarNivel(s.dataset.id, +s.value);
  });
  on('mutacion', (m) => { if (m.origen !== 'triaje') cargar(); });
  on('tab', () => { if (state.tabActual === 'triaje') cargar(); });
  setInterval(() => {
    const abierto = document.activeElement && document.activeElement.classList.contains('nivel');
    if (state.tabActual === 'triaje' && !despachando && !abierto) cargar();
  }, 4000);
  cargar();
}

export { cargar as cargarTriaje };
