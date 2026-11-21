# Diseño: Frontend web EHR + servidor HTTP embebido en C++

## 1. Hallazgos del backend actual

| Tema | Estado real en el código |
|---|---|
| Interfaz de entrada | Solo menú de consola (`src/main.cpp`). No hay servidor, puerto ni endpoints propios. |
| HTTP | `MongoAPI.h` usa WinHTTP **solo como cliente** hacia `https://paginas-web-cr.com/Api/apis/mongodb.php` (colección `pacientes_ehr`; GET/POST/PUT/DELETE). |
| JSON | Parser/serializador manual en `namespace JSON` (`MongoAPI.h`). Serializa: `idPaciente, nombre, tipoSangre, alergias, nivelTriage, scoreSOFA, enUCI`. |
| Compilación | `compilar.ps1`: g++ MSYS2 ucrt64, C++17, `-lwinhttp`. |
| Tabla Hash | `TablaHash.h`: linear hashing, 8 buckets iniciales, carga máx. 0.75, `nivel`/`sp` privados. Contiene **todos** los pacientes. |
| Árbol Rojo-Negro | `ArbolRojoNegro.h`: contiene **solo pacientes en UCI**, ordenado por `scoreSOFA` y luego `idPaciente`. Nodos con color, sin altura almacenada. |
| Max-Heap | `ColaPrioridad.h`: arreglo; raíz = menor `nivelTriage`, desempate por mayor SOFA y luego id. |
| Fachada | `EHRManager.h`: insertar, buscarPorId (hash), eliminar, actualizarGravedad, despacharTriage, rankingUCI, listarTodos. |

### Diferencias con lo descrito en el enunciado (requieren decisión)

1. **No existe un Grafo** (ni `derivaciones`) en el repositorio. Hay que crearlo.
2. **No existe el campo `edad` ni `diagnostico` a nivel de paciente.** `HistoriaClinica.diagnostico` existe, pero `Paciente.historias` no se serializa a Mongo ni al CSV. La "prioridad" actual es `nivelTriage` (1-5) + `scoreSOFA`.
3. **Bug latente en el Árbol Rojo-Negro:** el árbol se ordena por `(scoreSOFA, id)`, pero `buscarNodo`, `buscar` y `eliminar` navegan comparando solo `idPaciente`. La búsqueda y eliminación por id solo funcionan por casualidad. Esto afecta a `EHRManager::eliminar`, `actualizarGravedad` y `despacharTriage` para pacientes UCI, y haría inconsistente el visualizador. Debe corregirse (p. ej. `buscarPorId` con recorrido O(n), o buscar por clave `(SOFA, id)` pasando el SOFA conocido desde el hash).
4. **El Hash y el Árbol no indexan lo mismo.** "Buscar por Árbol RN o por Hash" solo es equivalente para pacientes UCI. El buscador debe mostrar esto explícitamente (el árbol responde "no está en UCI" para el resto).
5. `ColaPrioridad` y `ArbolRojoNegro` calculan `tamanio()`/búsqueda por id en O(n) en algunos caminos; el visualizador debe mostrar el costo real y no el teórico.
6. `despacharTriage()` elimina al paciente también del hash, del árbol y de Mongo (es "atender y dar de alta"). La UI debe indicarlo.

## 2. Cómo se comunicará el frontend con C++

**Decisión propuesta: servidor HTTP/1.1 embebido en el mismo `ehr_sistema.exe`, con WinSock2 (nativo de Windows, `-lws2_32`), sirviendo API REST JSON y los archivos estáticos de `web/`.**

- Mismo origen (`http://127.0.0.1:8080`): sin CORS, sin Node, sin dependencias externas.
- Enlace solo a `127.0.0.1` (no expuesto a la red).
- Un solo hilo de aceptación y procesamiento secuencial: las estructuras no son thread-safe y así no se necesitan locks. Las llamadas a Mongo ya son síncronas, por lo que se registran con duración.
- El menú de consola actual se conserva. Se activa el servidor con `ehr_sistema.exe --server [--port 8080]`; sin argumentos se comporta como hoy.
- Alternativas descartadas: WebSocket (necesita handshake SHA-1/Base64, complejidad innecesaria); proxy Node/Python (agrega dependencia y rompe "sin dependencias externas"); stdout JSON por pipe (requiere proceso padre).
- "Flujo asincrónico": el frontend usa `fetch` async + sondeo de `/api/eventos?desde=N` (log incremental) cada 1-2 s. SSE queda como mejora opcional.

### Archivos nuevos en C++ (sin modificar el comportamiento existente)

| Archivo | Responsabilidad |
|---|---|
| `include/HttpServer.h` | Socket, parseo de petición (método, ruta, query, body), respuesta, servir estáticos, CORS no necesario. |
| `include/Serializador.h` | Paciente/estructuras → JSON (reutiliza `JSON::escapar`, `JSON::getString`, etc.). |
| `include/ApiRouter.h` | Mapea rutas a `EHRManager`. |
| `include/Grafo.h` | Grafo dirigido de derivaciones (lista de adyacencia, BFS/ruta). |
| `include/EventLog.h` | Buffer circular de eventos (op, estructura, duración, estado Mongo). |

Cambios mínimos en archivos existentes: métodos **de solo lectura** `exportarJson()` en `TablaHash`, `ArbolRojoNegro`, `ColaPrioridad`; `EHRManager` expone accesores a esas estructuras y registra eventos; `main.cpp` agrega el flag `--server`; `compilar.ps1` agrega `-lws2_32`.

## 3. Contrato de la API (JSON, UTF-8)

Respuesta de toda operación de escritura incluye `ok`, `mensaje` y `trace` (qué estructuras tocó y qué cambió) para que el visualizador anime la operación sin recargar todo.

| Método y ruta | Descripción |
|---|---|
| `GET /api/estado` | `{backend:"online", nube:true/false, pacientes, uci, triage, hash:{buckets,carga}, uptime}` |
| `GET /api/pacientes` | Lista completa (desde hash). |
| `GET /api/pacientes/{id}?via=hash\|arbol` | Busca por la estructura elegida. Devuelve `{encontrado, via, pasos:[...], comparaciones, microsegundos, paciente}`. |
| `POST /api/pacientes` | Inserta. Body: `{idPaciente,nombre,edad?,tipoSangre,alergias[],diagnostico?,nivelTriage,scoreSOFA,enUCI}`. |
| `PUT /api/pacientes/{id}` | Actualiza gravedad/campos (usa `actualizarGravedad`). |
| `DELETE /api/pacientes/{id}` | Elimina de todas las estructuras y de Mongo. |
| `GET /api/triage` | Heap como arreglo: `[{pos, idPaciente, nombre, nivelTriage, scoreSOFA}]` + `siguiente`. |
| `POST /api/triage/despachar` | Extrae el más urgente; devuelve paciente y el heap resultante. |
| `GET /api/estructuras/arbol` | Árbol anidado `{id, nombre, sofa, color, altura, alturaNegra, izq, der}` + recorridos `inorden, preorden, postorden`. |
| `GET /api/estructuras/hash` | `{nivel, sp, buckets:[[ids...]], carga, elementos, ultimoSplit}`. |
| `GET /api/estructuras/heap` | Arreglo + aristas padre-hijo implícitas. |
| `GET /api/grafo` / `POST /api/grafo/derivaciones` | Nodos (centros/especialistas) y aristas (derivaciones). |
| `GET /api/eventos?desde=N` | Log incremental: `{id, ts, metodo, ruta, estructura, ms, mongo:{estado,http}}`. |

Notas: "factor de balanceo" en un Rojo-Negro no es un campo propio; se mostrará **altura** y **altura negra** (calculadas al exportar). `ultimoSplit` se detecta comparando `buckets()` antes y después de cada inserción.

## 4. Estructura del frontend (`web/`, sin build)

```
web/
  index.html            # shell: topbar + pestañas
  css/app.css           # Tailwind via CLI no; CSS propio con variables (sin CDN)
  js/api.js             # fetch + manejo de errores + estado de conexión
  js/store.js           # estado y eventos (pub/sub)
  js/ui/gestion.js      # CRUD y buscador (hash vs árbol)
  js/ui/triage.js       # cola de urgencias
  js/ui/estado.js       # indicador backend/Mongo + log
  js/viz/arbol.js       # SVG Rojo-Negro + recorridos
  js/viz/hash.js        # buckets, carga, split
  js/viz/heap.js        # árbol binario + arreglo
  js/viz/grafo.js       # SVG, layout circular/fuerza simple
```

JavaScript Vanilla con módulos ES. **Sin CDN**: Tailwind por CDN requiere internet y contradice "sin dependencias"; se usará CSS propio con variables (blanco clínico, gris pizarra, azul médico; rojo/ámbar para triaje y nodos).

## 5. Riesgos
- Mongo ignora errores de certificado y es síncrono: una caída de red alarga cada petición. Mitigación: timeouts en WinHTTP y registro de duración.
- Datos Mongo existentes sin `edad`/`diagnostico`: campos opcionales.
- Corregir el bug del árbol es prerequisito del visualizador.
