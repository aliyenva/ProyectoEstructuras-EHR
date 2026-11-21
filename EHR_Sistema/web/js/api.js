// api.js · Cliente de la API REST del backend C++ (mismo origen, sin CORS)
import { setBackend } from './store.js';

/**
 * Llama al backend. Devuelve el JSON (con ok:true) o lanza Error con el
 * mensaje del servidor. Si el servidor no responde marca el backend offline.
 */
export async function api(metodo, ruta, cuerpo) {
  let res;
  try {
    res = await fetch(ruta, {
      method: metodo,
      headers: cuerpo ? { 'Content-Type': 'application/json' } : {},
      body: cuerpo ? JSON.stringify(cuerpo) : undefined,
    });
  } catch (e) {
    setBackend(false);
    throw new Error('Sin conexión con el backend C++ (¿está corriendo ehr_sistema.exe --server?)');
  }
  setBackend(true);

  let datos;
  try { datos = await res.json(); }
  catch { throw new Error(`Respuesta no válida del servidor (HTTP ${res.status})`); }

  if (!datos.ok) {
    const err = new Error(datos.error || `Error HTTP ${res.status}`);
    err.status = res.status;
    throw err;
  }
  return datos;
}

export const get  = (ruta)        => api('GET', ruta);
export const post = (ruta, datos) => api('POST', ruta, datos || {});
export const put  = (ruta, datos) => api('PUT', ruta, datos || {});
export const del  = (ruta)        => api('DELETE', ruta);
export const enc  = encodeURIComponent;
