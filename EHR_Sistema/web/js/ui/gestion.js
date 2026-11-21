// ui/gestion.js · Panel de gestión clínica: alta/edición, buscador y tabla (CRUD)
import { get, post, put, del, enc } from '../api.js';
import { emit, on } from '../store.js';
import { $, $$, esc, debounce, toast, badgeNivel, chipsTrace } from '../util.js';

let pacientes = [];       // última lista recibida
let editando = null;      // id en edición, o null (alta)
let pacienteEdit = null;  // objeto original en edición

const COMPONENTES = [
  ['respiratorio', 'Respiratorio'], ['coagulacion', 'Coagulación'], ['hepatico', 'Hepático'],
  ['cardiovascular', 'Cardiovascular'], ['neurologico', 'Neurológico'], ['renal', 'Renal'],
];

// ── Parámetros SOFA del formulario ───────────────────────────────
function leerParametros() {
  return {
    pao2fio2: +$('#s-pao2fio2').value,
    ventilado: $('#s-ventilado').checked,
    plaquetas: +$('#s-plaquetas').value,
    bilirrubina: +$('#s-bilirrubina').value,
    pam: +$('#s-pam').value,
    vasopresor: +$('#s-vasopresor').value,
    glasgow: +$('#s-glasgow').value,
    creatinina: +$('#s-creatinina').value,
  };
}

function escribirParametros(p) {
  $('#s-pao2fio2').value = p.pao2fio2;
  $('#s-ventilado').checked = !!p.ventilado;
  $('#s-plaquetas').value = p.plaquetas;
  $('#s-bilirrubina').value = p.bilirrubina;
  $('#s-pam').value = p.pam;
  $('#s-vasopresor').value = p.vasopresor;
  $('#s-glasgow').value = p.glasgow;
  $('#s-creatinina').value = p.creatinina;
}

/** Pide el cálculo al backend (única fuente de verdad del SOFA) */
async function calcularSOFA() {
  try {
    const r = await post('/api/sofa/calcular', leerParametros());
    const total = $('#sofa-total');
    total.textContent = r.total;
    total.parentElement.className = 'sofa-total' + (r.total >= 12 ? ' alto' : r.total >= 6 ? ' medio' : '');
    $('#sofa-bars').innerHTML = COMPONENTES.map(([k, nombre]) => {
      const v = r.componentes[k];
      return `<div class="bar-row"><span>${nombre}</span><div class="bar"><i class="${v >= 2 ? 'l' + v : ''}" style="width:${v * 25}%"></i></div><b>${v}</b></div>`;
    }).join('');
  } catch (e) {
    $('#sofa-bars').innerHTML = `<span class="muted small">${esc(e.message)}</span>`;
  }
}
const calcularSOFADebounce = debounce(calcularSOFA, 250);

// ── Formulario ───────────────────────────────────────────────────
function limpiarFormulario() {
  editando = null; pacienteEdit = null;
  $('#form-paciente').reset();
  $('#f-id').readOnly = false;
  $('#form-titulo').textContent = 'Nuevo paciente';
  $('#btn-guardar').textContent = 'Registrar paciente';
  $('#btn-cancelar-edicion').classList.add('hidden');
  $('#row-usar-params').classList.add('hidden');
  $('#sofa-grid').classList.remove('off');
  $('#trace-form').classList.add('hidden');
  escribirParametros({ pao2fio2: 400, ventilado: false, plaquetas: 200, bilirrubina: 0.5, pam: 80, vasopresor: 0, glasgow: 15, creatinina: 0.8 });
  calcularSOFA();
}

function editar(id) {
  const p = pacientes.find((x) => x.idPaciente === id);
  if (!p) return;
  editando = id; pacienteEdit = p;
  $('#f-id').value = p.idPaciente; $('#f-id').readOnly = true;
  $('#f-nombre').value = p.nombre;
  $('#f-edad').value = p.edad;
  $('#f-sangre').value = p.tipoSangre || 'O+';
  $('#f-diag').value = p.diagnostico;
  $('#f-alergias').value = (p.alergias || []).join(', ');
  $('#f-triage').value = p.nivelTriage;
  $('#f-uci').checked = p.enUCI;
  escribirParametros(p.sofaParams);

  $('#form-titulo').textContent = `Editar paciente ${p.idPaciente}`;
  $('#btn-guardar').textContent = 'Guardar cambios';
  $('#btn-cancelar-edicion').classList.remove('hidden');

  // Pacientes antiguos: SOFA ingresado a mano, sin parámetros clínicos
  const manual = !p.sofaMedido;
  $('#row-usar-params').classList.toggle('hidden', !manual);
  $('#f-usar-params').checked = false;
  $('#sofa-manual-nota').textContent = manual ? `(SOFA actual ${p.scoreSOFA}, registrado manualmente)` : '';
  $('#sofa-grid').classList.toggle('off', manual);
  calcularSOFA();
  $('#form-paciente').scrollIntoView({ behavior: 'smooth', block: 'start' });
}

function cuerpoFormulario() {
  const b = {
    idPaciente: $('#f-id').value.trim(),
    nombre: $('#f-nombre').value.trim(),
    edad: +$('#f-edad').value || 0,
    tipoSangre: $('#f-sangre').value,
    diagnostico: $('#f-diag').value.trim(),
    alergias: $('#f-alergias').value.split(',').map((s) => s.trim()).filter(Boolean),
    nivelTriage: +$('#f-triage').value,
    enUCI: $('#f-uci').checked,
  };
  // Alta: siempre SOFA automático. Edición: automático salvo paciente antiguo
  // con SOFA manual, donde el usuario decide con la casilla.
  const usarParams = !editando ? true : (pacienteEdit.sofaMedido ? true : $('#f-usar-params').checked);
  if (usarParams) Object.assign(b, leerParametros(), { sofaMedido: true });
  else b.sofaMedido = false;       // conserva el SOFA manual existente
  return b;
}

function mostrarTrace(trace) {
  const t = $('#trace-form');
  t.innerHTML = '<strong class="small muted" style="width:100%">Efecto en las estructuras:</strong>' + chipsTrace(trace);
  t.classList.remove('hidden');
}

async function guardar(ev) {
  ev.preventDefault();
  const btn = $('#btn-guardar');
  btn.disabled = true;
  try {
    const b = cuerpoFormulario();
    const r = editando
      ? await put(`/api/pacientes/${enc(editando)}`, b)
      : await post('/api/pacientes', b);
    toast(`${r.mensaje}: ${r.paciente.nombre} · SOFA ${r.paciente.scoreSOFA}`);
    const id = r.paciente.idPaciente;
    const tipo = editando ? 'actualizar' : 'insertar';
    limpiarFormulario();
    mostrarTrace(r.trace);
    emit('mutacion', { tipo, id, trace: r.trace, origen: 'gestion' });
    await cargarLista(id);
  } catch (e) {
    toast(e.message, 'err');
  } finally {
    btn.disabled = false;
  }
}

// ── Tabla ────────────────────────────────────────────────────────
function pintarTabla(resaltar) {
  const tb = $('#tabla-pacientes tbody');
  if (!pacientes.length) {
    tb.innerHTML = '<tr><td colspan="8" class="empty">Sin resultados</td></tr>';
    return;
  }
  tb.innerHTML = pacientes.map((p) => `
    <tr class="${p.idPaciente === resaltar ? 'flash' : ''}">
      <td><b>${esc(p.idPaciente)}</b></td>
      <td>${esc(p.nombre)}</td>
      <td>${p.edad || '–'}</td>
      <td>${badgeNivel(p.nivelTriage)}</td>
      <td>${esc(p.scoreSOFA)}<span class="tag ${p.sofaMedido ? 'auto' : 'manual'}">${p.sofaMedido ? 'auto' : 'manual'}</span></td>
      <td>${p.enUCI ? '<span class="uci-si">UCI</span>' : '–'}</td>
      <td>${esc(p.diagnostico) || '<span class="muted">–</span>'}</td>
      <td class="row-actions">
        <button class="btn-secondary" data-editar="${esc(p.idPaciente)}">Editar</button>
        <button class="btn-secondary" data-eliminar="${esc(p.idPaciente)}">Eliminar</button>
      </td>
    </tr>`).join('');
}

async function cargarLista(resaltar) {
  const q = $('#q').value.trim();
  try {
    const r = await get(`/api/pacientes${q ? '?q=' + enc(q) : ''}`);
    pacientes = r.pacientes;
    $('#cnt-pacientes').textContent = q ? `(${pacientes.length} de ${r.total})` : `(${r.total})`;
    pintarTabla(resaltar);
  } catch (e) {
    $('#tabla-pacientes tbody').innerHTML = `<tr><td colspan="8" class="empty">${esc(e.message)}</td></tr>`;
  }
}

async function eliminar(id) {
  if (!confirm(`¿Eliminar el expediente ${id}? Se borra del hash, del árbol, del heap y de MongoDB.`)) return;
  try {
    const r = await del(`/api/pacientes/${enc(id)}`);
    toast(`Paciente ${id} eliminado`);
    if (editando === id) limpiarFormulario();
    mostrarTrace(r.trace);
    emit('mutacion', { tipo: 'eliminar', id, trace: r.trace, origen: 'gestion' });
    await cargarLista();
    buscar();
  } catch (e) { toast(e.message, 'err'); }
}

// ── Buscador ─────────────────────────────────────────────────────
function viaActual() { return $('input[name="via"]:checked').value; }

async function buscar() {
  const q = $('#q').value.trim();
  const caja = $('#resultado-busqueda');
  await cargarLista();
  if (!q) { caja.classList.add('hidden'); return; }

  const via = viaActual();
  let url = `/api/pacientes/${enc(q)}?via=${via}`;
  const sofa = $('#q-sofa').value;
  if (via === 'arbol' && sofa !== '') url += `&sofa=${enc(sofa)}`;
  try {
    const r = await get(url);
    const nombreVia = r.via === 'hash' ? 'Tabla Hash' : 'Árbol Rojo-Negro';
    const pasos = r.pasos.map((p) => `<li>${esc(p)}</li>`).join('');
    caja.className = 'resultado ' + (r.encontrado ? 'ok' : 'no');
    caja.innerHTML = `
      <h4>${r.encontrado ? '✔ Encontrado' : '✘ No encontrado'} mediante ${nombreVia}</h4>
      <div class="metrics">
        <span class="metric">Complejidad: ${esc(r.complejidad)}</span>
        <span class="metric">Comparaciones: ${r.comparaciones}</span>
        <span class="metric">Tiempo: ${esc(r.microsegundos)} µs</span>
        ${r.bucket !== undefined ? `<span class="metric">Bucket: ${r.bucket}</span>` : ''}
      </div>
      ${r.encontrado ? `<div>${esc(r.paciente.nombre)} · ${badgeNivel(r.paciente.nivelTriage)} · SOFA ${esc(r.paciente.scoreSOFA)}
        <button class="btn-link" data-editar="${esc(r.paciente.idPaciente)}">Editar</button>
        <button class="btn-link" data-eliminar="${esc(r.paciente.idPaciente)}">Eliminar</button></div>` : ''}
      ${r.nota ? `<p class="small"><b>Nota:</b> ${esc(r.nota)}</p>` : ''}
      <ol>${pasos}</ol>`;
    caja.classList.remove('hidden');
  } catch (e) {
    caja.className = 'resultado no';
    caja.innerHTML = esc(e.message);
    caja.classList.remove('hidden');
  }
}
const buscarDebounce = debounce(buscar, 300);

// ── Inicialización ───────────────────────────────────────────────
export function iniciarGestion() {
  $('#form-paciente').addEventListener('submit', guardar);
  $('#btn-limpiar').addEventListener('click', limpiarFormulario);
  $('#btn-cancelar-edicion').addEventListener('click', limpiarFormulario);
  $('#btn-refrescar').addEventListener('click', () => cargarLista());

  $$('#sofa-grid input, #sofa-grid select').forEach((el) => {
    el.addEventListener('input', calcularSOFADebounce);
    el.addEventListener('change', calcularSOFADebounce);
  });
  $('#f-usar-params').addEventListener('change', (e) => {
    $('#sofa-grid').classList.toggle('off', !e.target.checked);
  });

  $('#q').addEventListener('input', buscarDebounce);
  $('#q-sofa').addEventListener('input', buscarDebounce);
  $$('input[name="via"]').forEach((r) => r.addEventListener('change', () => {
    $('#lbl-sofa-clave').classList.toggle('hidden', viaActual() !== 'arbol');
    buscar();
  }));

  // Delegación de acciones en tabla y resultado
  document.addEventListener('click', (ev) => {
    const ed = ev.target.closest('[data-editar]');
    if (ed) { editar(ed.dataset.editar); return; }
    const el = ev.target.closest('[data-eliminar]');
    if (el) eliminar(el.dataset.eliminar);
  });

  // Si otra pestaña modifica datos (triaje, simulador), refrescar la lista
  on('mutacion', (m) => { if (m.origen !== 'gestion') cargarLista(); });

  limpiarFormulario();
  cargarLista();
}

export { cargarLista };
