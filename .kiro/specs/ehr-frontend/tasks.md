# Plan de implementación

- [ ] 1. Corregir búsqueda/eliminación por id en `ArbolRojoNegro` (clave compuesta SOFA+id).
- [ ] 2. Añadir `EventLog.h` y registrar eventos en `EHRManager` (incluye estado y duración de llamadas a Mongo).
- [ ] 3. Añadir exportación JSON de solo lectura a `TablaHash`, `ArbolRojoNegro` (altura, altura negra, recorridos) y `ColaPrioridad`.
- [ ] 4. Detectar splits del hash (buckets antes/después) y exponer `ultimoSplit`.
- [ ] 5. Crear `Grafo.h` (derivaciones) y su exportación JSON.
- [ ] 6. Ampliar `Paciente` con `edad` y `diagnostico` (opcionales) y su serialización Mongo/CSV, compatible con datos antiguos.
- [ ] 7. Implementar `HttpServer.h` (WinSock2, 127.0.0.1) y `ApiRouter.h` con el contrato de `design.md`.
- [ ] 8. Flag `--server` en `main.cpp`; agregar `-lws2_32` a `compilar.ps1`; el menú de consola sigue funcionando.
- [ ] 9. Frontend: shell, topbar de estado y log de peticiones.
- [ ] 10. Frontend: panel CRUD y buscador (hash vs árbol, con pasos y tiempos).
- [ ] 11. Frontend: cola de triaje y despacho.
- [ ] 12. Frontend: visualizadores (árbol, hash, heap, grafo) con refresco tras cada operación.
- [ ] 13. README con pasos para compilar y abrir `http://127.0.0.1:8080`.
