# EHR · Plataforma de Salud Digital

Backend C++17 (Árbol Rojo-Negro, Hash lineal, Max-Heap, Grafo, MongoDB) con interfaz web.
Sin dependencias externas: el `.exe` incluye su propio servidor HTTP (WinSock2) y sirve la carpeta `web/`.

## Compilar (MinGW / MSYS2 ucrt64)

```powershell
cd EHR_Sistema
.\compilar.ps1
```

Comando equivalente:

```powershell
g++ -std=c++17 -Wall -O2 -Iinclude src\main.cpp -o build\ehr_sistema.exe -lwinhttp -lws2_32
```

## Ejecutar

| Modo | Comando |
|---|---|
| Consola (menú original) | `cd build` → `.\ehr_sistema.exe` |
| Web | `cd build` → `.\ehr_sistema.exe --server` y abrir <http://127.0.0.1:8080> |

Opciones: `--port 8090` (otro puerto), `--web <ruta>` (carpeta del frontend; por defecto busca `web`, `..\web`).
El servidor solo escucha en `127.0.0.1`. Ejecute siempre desde `build\` (lee `data\pacientes.csv`).
Si el puerto 8080 está ocupado, use `--port`.

## SOFA automático

El score SOFA ya no se escribe a mano: se calcula con seis parámetros clínicos (PaO₂/FiO₂ con ventilación,
plaquetas, bilirrubina, PAM/vasopresores, Glasgow, creatinina), 0-4 puntos por sistema, total 0-24
(`include/SOFA.h`). Aplica en consola y web. Los pacientes antiguos (CSV/Mongo sin parámetros) conservan su SOFA
manual hasta que se editen y se active "Calcular SOFA con estos parámetros".

## Interfaz web

- **Gestión clínica**: alta/edición/baja, buscador (por Tabla Hash, o por Árbol Rojo-Negro; solo UCI),
  con pasos, comparaciones y tiempo en µs. Cada operación muestra su efecto en las estructuras.
- **Triaje**: cola del Max-Heap, despacho con flujo paso a paso, cambio de nivel con reubicación.
- **Estructuras de datos**: árbol Rojo-Negro (colores, altura, altura negra, balance, recorridos animados),
  Hash lineal (buckets, colisiones, carga, splits), Max-Heap (árbol y arreglo, intercambios),
  Grafo de derivaciones (editable, ruta más corta, alcanzables, derivar pacientes; solo en memoria).
- **Barra superior**: estado del backend C++ y de MongoDB, última petición, y bitácora de peticiones y sincronizaciones.

Enlaces directos: `#triaje`, `#estructuras/arbol|hash|heap|grafo`.

## API (JSON)

`GET /api/estado` · `GET|POST /api/pacientes` · `GET|PUT|DELETE /api/pacientes/{id}?via=hash|arbol[&sofa=x]` ·
`POST /api/sofa/calcular` · `GET /api/triage` · `POST /api/triage/despachar` ·
`GET /api/estructuras/{arbol|hash|heap}` · `GET /api/grafo` (+ `nodos`, `aristas`, `ruta`, `alcanzables`, `derivar`) ·
`GET /api/eventos?desde=N` · `POST /api/nube/reconectar`.
Diseño completo en `../.kiro/specs/ehr-frontend/design.md`.

## Cambios en el backend

- `ArbolRojoNegro`: corregida la búsqueda/eliminación (el árbol se ordena por `(SOFA, id)`; antes navegaba solo por id).
  Eliminar y buscar por clave son O(log n); solo con el id se recorre el árbol en O(n).
- `Paciente`: nuevos campos opcionales `edad`, `diagnostico` y parámetros SOFA (compatibles con datos existentes).
- Llamadas a MongoDB con timeouts (5-15 s) y registro en la bitácora.
