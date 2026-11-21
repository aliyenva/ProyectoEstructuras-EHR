// viz/hash.js · Tabla Hash lineal: buckets, colisiones, factor de carga y splits
import { get } from '../api.js';
import { $, esc } from '../util.js';

let splitsVistos = null;   // para detectar un split nuevo entre refrescos

export async function refrescarHash() {
  let h;
  try { h = (await get('/api/estructuras/hash')).hash; }
  catch (e) { $('#hash-buckets').innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }

  const pct = Math.min(100, (h.carga / 1) * 100);
  const umbral = h.cargaMax * 100;
  $('#hash-stats').innerHTML = `
    <div class="stat-card"><b>${h.elementos}</b><span>Registros</span></div>
    <div class="stat-card"><b>${h.buckets}</b><span>Buckets</span></div>
    <div class="stat-card"><b>${h.carga}</b><span>Factor de carga (máx ${h.cargaMax})</span>
      <div class="loadbar"><i class="${h.carga > h.cargaMax * 0.9 ? 'alta' : ''}" style="width:${pct}%"></i><u style="left:${umbral}%"></u></div></div>
    <div class="stat-card"><b>${h.nivel}</b><span>Nivel (h = hash mod 2^${h.nivel})</span></div>
    <div class="stat-card"><b>${h.sp}</b><span>Puntero de split (sp)</span></div>
    <div class="stat-card"><b>${h.colisiones}</b><span>Colisiones (cadena &gt; 1)</span></div>
    <div class="stat-card"><b>${h.cadenaMax}</b><span>Cadena más larga</span></div>
    <div class="stat-card"><b>${h.splitsTotales}</b><span>Splits acumulados</span></div>`;

  const sp = h.ultimoSplit;
  const nuevoSplit = splitsVistos !== null && h.splitsTotales > splitsVistos;
  const banner = $('#hash-banner');
  if (sp) {
    banner.className = 'banner' + (nuevoSplit ? ' split' : '');
    banner.textContent = (nuevoSplit ? '¡Split dinámico! ' : 'Último split: ')
      + `el bucket ${sp.bucket} se dividió y creó el bucket ${sp.nuevoBucket} (nivel ${sp.nivel} → h usa 2^${sp.nivel + 1}); `
      + `${sp.movidos} registro(s) se movieron al bucket nuevo. Se dispara cuando la carga supera ${h.cargaMax}.`;
  } else {
    banner.className = 'banner';
    banner.textContent = `Aún no hubo splits. Ocurrirá uno cuando el factor de carga supere ${h.cargaMax}.`;
  }
  splitsVistos = h.splitsTotales;

  $('#hash-buckets').innerHTML = h.tabla.map((b) => {
    const clase = b.items.length === 0 ? 'vacio' : b.items.length > 1 ? 'colision' : '';
    const resaltado = nuevoSplit && sp && (b.indice === sp.bucket || b.indice === sp.nuevoBucket) ? ' nuevo' : '';
    const items = b.items.length
      ? b.items.map((i) => `<span class="bitem" title="FNV-1a = ${i.hash}&#10;${esc(i.nombre)}">${esc(i.id)}</span>`).join('')
      : '<span class="bempty">(vacío)</span>';
    return `<div class="bucket ${clase}${resaltado}">
      <div class="bidx">[${b.indice}]${b.puntero ? '<em>▶ sp</em>' : ''}</div>
      <div class="bitems">${items}</div>
      ${b.items.length > 1 ? `<small>colisión ×${b.items.length}</small>` : ''}
    </div>`;
  }).join('');
}
