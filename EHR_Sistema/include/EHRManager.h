#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "Paciente.h"
#include "TablaHash.h"
#include "ArbolRojoNegro.h"
#include "ColaPrioridad.h"
#include "MongoAPI.h"
#include "CargadorDatos.h"
#include "Grafo.h"
#include "EventLog.h"

// ================================================================
//  EHRManager.h  –  Fachada del sistema EHR
//  Integra las tres estructuras en memoria + API MongoDB.
//
//  Regla de consistencia:
//    - TablaHash    → TODOS los pacientes (búsqueda O(1) por ID)
//    - ArbolRN      → SOLO pacientes con enUCI = true
//                     (clave: scoreSOFA + id)
//    - ColaPrioridad→ TODOS los pacientes (triage global)
//    - MongoDB      → persistencia; se sincroniza en cada operación
//    - Grafo        → red de derivaciones (solo en memoria)
//
//  El SOFA se calcula automaticamente a partir de los parametros
//  clinicos (ver SOFA.h) cada vez que se inserta o actualiza.
// ================================================================

class EHRManager {
private:
    TablaHash      hashPacientes;
    ArbolRojoNegro arbolUCI;
    ColaPrioridad  colaTriage;
    MongoAPI       mongoAPI;
    Grafo          grafoDerivaciones;
    EventLog       bitacora;
    bool           nubeActiva;
    bool           ultimoSyncOk;

    // ── Registra la ultima llamada a Mongo en la bitacora ─────────
    void registrarMongo(const std::string& operacion, bool ok) {
        Evento e;
        e.tipo       = "mongo";
        e.metodo     = mongoAPI.ultimoMetodo;
        e.ruta       = "pacientes_ehr";
        e.estructura = "mongo";
        e.codigo     = (int)mongoAPI.ultimoCodigo;
        e.ms         = mongoAPI.ultimoMs;
        e.ok         = ok;
        e.mensaje    = operacion + (ok ? " sincronizado" : " FALLO (sin respuesta o error)");
        bitacora.agregar(e);
        ultimoSyncOk = ok;
    }

    // ── Sincronizar inserción con MongoDB ─────────────────────────
    std::string sincronizarInsercion(Paciente& p) {
        if (!nubeActiva) return "";
        std::string id = mongoAPI.insertar(p);
        // Exito = HTTP 2xx (la API puede no devolver el _id en el cuerpo)
        registrarMongo("INSERT " + p.idPaciente,
                       mongoAPI.ultimoCodigo >= 200 && mongoAPI.ultimoCodigo < 300);
        if (!id.empty()) p.mongoId = id;
        return id;
    }

    // ── Sincronizar actualización con MongoDB ─────────────────────
    void sincronizarActualizacion(const Paciente& p) {
        if (!nubeActiva) return;
        bool ok = mongoAPI.actualizar(p);
        registrarMongo("UPDATE " + p.idPaciente, ok);
    }

    // ── Sincronizar eliminación con MongoDB ───────────────────────
    void sincronizarEliminacion(const std::string& idPaciente) {
        if (!nubeActiva) return;
        bool ok = mongoAPI.eliminar(idPaciente);
        registrarMongo("DELETE " + idPaciente, ok);
    }

public:
    // ── Constructor: carga datos al iniciar ───────────────────────
    explicit EHRManager(bool verbose = false)
        : mongoAPI(verbose), nubeActiva(false), ultimoSyncOk(false) {}

    // ── Inicialización del sistema ────────────────────────────────
    void inicializar() {
        std::cout << "\n";
        std::cout << "  ╔══════════════════════════════════════════╗\n";
        std::cout << "  ║   PLATAFORMA EHR – SALUD DIGITAL         ║\n";
        std::cout << "  ║   Estructuras de Datos II                ║\n";
        std::cout << "  ╚══════════════════════════════════════════╝\n\n";

        nubeActiva   = mongoAPI.estaConectado();
        ultimoSyncOk = nubeActiva;
        registrarMongo("CONEXION inicial", nubeActiva);
        if (nubeActiva)
            std::cout << "  [EHR] Estado de nube: ✔ CONECTADO\n";
        else
            std::cout << "  [EHR] Estado de nube: ✘ OFFLINE (modo local)\n";

        auto resultado = CargadorDatos::cargar(
            hashPacientes, arbolUCI, colaTriage, mongoAPI);

        Evento e;
        e.tipo = "sistema"; e.estructura = "sistema"; e.ok = true;
        e.mensaje = "Sistema listo: " + std::to_string(resultado.cargados)
                  + " paciente(s) desde " + resultado.fuente;
        bitacora.agregar(e);

        std::cout << "  [EHR] Sistema listo. " << resultado.cargados
                  << " paciente(s) en memoria.\n\n";
    }

    // ── Reintentar la conexion con la nube ────────────────────────
    bool reconectarNube() {
        nubeActiva   = mongoAPI.estaConectado();
        ultimoSyncOk = nubeActiva;
        registrarMongo("RECONEXION", nubeActiva);
        return nubeActiva;
    }

    // ================================================================
    //  OPERACIONES CRUD
    // ================================================================

    // ── Insertar nuevo paciente ───────────────────────────────────
    bool insertar(Paciente p) {
        // Validar datos mínimos
        if (p.idPaciente.empty() || p.nombre.empty()) {
            std::cout << "  [EHR] Paciente inválido (ID o nombre vacío), descartado.\n";
            return false;
        }
        // Normalizar nivelTriage (0 no es válido, default 5)
        if (p.nivelTriage < 1 || p.nivelTriage > 5) p.nivelTriage = 5;
        // Verificar duplicado
        if (hashPacientes.buscar(p.idPaciente)) {
            std::cout << "  [EHR] Ya existe un paciente con ID: " << p.idPaciente << "\n";
            return false;
        }

        p.recalcularSOFA();       // SOFA automatico
        sincronizarInsercion(p);  // asigna mongoId si hay conexión
        hashPacientes.insertar(p);
        colaTriage.insertar(p);
        if (p.enUCI) arbolUCI.insertar(p);

        std::cout << "  [EHR] Paciente insertado: " << p.nombre
                  << " (ID=" << p.idPaciente << ", SOFA=" << p.scoreSOFA << ")\n";
        return true;
    }

    // ── Buscar paciente por ID (O(1) desde hash) ──────────────────
    Paciente* buscarPorId(const std::string& idPaciente) {
        return hashPacientes.buscar(idPaciente);
    }

    // ── Eliminar paciente ─────────────────────────────────────────
    bool eliminar(const std::string& idPaciente) {
        Paciente* p = hashPacientes.buscar(idPaciente);
        if (!p) {
            std::cout << "  [EHR] No se encontró paciente con ID: " << idPaciente << "\n";
            return false;
        }

        sincronizarEliminacion(idPaciente);
        bool   enUCI = p->enUCI;
        double sofa  = p->scoreSOFA;
        hashPacientes.eliminar(idPaciente);      // invalida 'p'
        colaTriage.eliminar(idPaciente);
        if (enUCI) arbolUCI.eliminar(sofa, idPaciente);

        std::cout << "  [EHR] Paciente eliminado: " << idPaciente << "\n";
        return true;
    }

    // ── Actualizar datos completos de un paciente ────────────────
    //  'nuevo' trae todos los campos editables. Se conservan mongoId e
    //  historias. Si nuevo.sofaParams.medido, el SOFA se recalcula.
    bool actualizar(const std::string& idPaciente, Paciente nuevo) {
        Paciente* p = hashPacientes.buscar(idPaciente);
        if (!p) {
            std::cout << "  [EHR] No encontrado: " << idPaciente << "\n";
            return false;
        }

        bool   eraUCI   = p->enUCI;
        double sofaViejo = p->scoreSOFA;
        // Sacar del arbol con la clave ANTIGUA (antes de cambiar el SOFA)
        if (eraUCI) arbolUCI.eliminar(sofaViejo, idPaciente);

        nuevo.idPaciente = idPaciente;
        nuevo.mongoId    = p->mongoId;
        nuevo.historias  = p->historias;
        if (nuevo.nivelTriage < 1 || nuevo.nivelTriage > 5) nuevo.nivelTriage = 5;
        nuevo.recalcularSOFA();

        *p = nuevo;                      // actualiza el hash (misma lista)
        colaTriage.actualizarPaciente(nuevo);
        if (nuevo.enUCI) arbolUCI.insertar(nuevo);

        sincronizarActualizacion(nuevo);

        std::cout << "  [EHR] Paciente actualizado: " << nuevo.nombre
                  << " → Triage " << nuevo.nivelTriage
                  << " | SOFA " << nuevo.scoreSOFA
                  << (nuevo.enUCI ? " | UCI" : "") << "\n";
        return true;
    }

    // ── Actualizar solo gravedad con SOFA manual (compatibilidad) ─
    bool actualizarGravedad(const std::string& idPaciente,
                             int    nuevoTriage,
                             double nuevoSOFA,
                             bool   pasaAUCI = false) {
        Paciente* p = hashPacientes.buscar(idPaciente);
        if (!p) return false;
        Paciente n = *p;
        n.nivelTriage = nuevoTriage;
        n.scoreSOFA   = nuevoSOFA;
        n.sofaParams.medido = false;     // SOFA manual
        n.enUCI       = pasaAUCI;
        return actualizar(idPaciente, n);
    }

    // ── Actualizar gravedad con SOFA AUTOMATICO ──────────────────
    bool actualizarGravedad(const std::string& idPaciente,
                             int nuevoTriage,
                             const ParametrosSOFA& params,
                             bool pasaAUCI) {
        Paciente* p = hashPacientes.buscar(idPaciente);
        if (!p) return false;
        Paciente n = *p;
        n.nivelTriage = nuevoTriage;
        n.sofaParams  = params;
        n.sofaParams.medido = true;
        n.enUCI       = pasaAUCI;
        return actualizar(idPaciente, n);
    }

    // ================================================================
    //  OPERACIONES ESPECIALES
    // ================================================================

    // ── Despachar siguiente paciente de Triage ────────────────────
    Paciente despacharTriage() {
        if (colaTriage.vacia())
            throw std::runtime_error("Cola de Triage vacía.");
        Paciente p = colaTriage.extraerUrgente();
        // Eliminar también de hash y árbol
        hashPacientes.eliminar(p.idPaciente);
        if (p.enUCI) arbolUCI.eliminar(p.scoreSOFA, p.idPaciente);
        sincronizarEliminacion(p.idPaciente);
        return p;
    }

    // ── Ranking UCI (más grave primero, por SOFA) ─────────────────
    std::vector<Paciente> rankingUCI() {
        return arbolUCI.rankingUCI();
    }

    // ── Ver próximo en Triage sin extraerlo ───────────────────────
    const Paciente& proximoTriage() {
        return colaTriage.verUrgente();
    }

    bool colaVacia() const { return colaTriage.vacia(); }

    // ── Listar todos los pacientes (desde hash) ───────────────────
    std::vector<Paciente> listarTodos() {
        return hashPacientes.todos();
    }

    // ================================================================
    //  VISUALIZACIONES
    // ================================================================

    void mostrarHash()         { hashPacientes.mostrar(); }
    void mostrarArbolUCI()     { arbolUCI.mostrar(); }
    void mostrarColaTriage()   { colaTriage.mostrar(); }

    void mostrarEstadisticas() const {
        std::cout << "\n  ┌── Estadísticas del Sistema ─────────────────\n";
        std::cout << "  │ Pacientes en hash      : " << hashPacientes.elementos() << "\n";
        std::cout << "  │   Buckets hash         : " << hashPacientes.buckets()
                  << "  (carga=" << hashPacientes.carga() << ")\n";
        std::cout << "  │ Pacientes en UCI (RBT) : " << arbolUCI.tamanio()   << "\n";
        std::cout << "  │ Pacientes en Triage    : " << colaTriage.tamanio() << "\n";
        std::cout << "  │ Nube MongoDB           : "
                  << (nubeActiva ? "✔ Activa" : "✘ Offline") << "\n";
        std::cout << "  └────────────────────────────────────────────\n\n";
    }

    bool nubeEstaActiva() const { return nubeActiva; }
    bool ultimoSincronizadoOk() const { return ultimoSyncOk; }

    // ── Accesos para la API web (estructuras y servicios) ─────────
    TablaHash&      hash()    { return hashPacientes; }
    ArbolRojoNegro& arbol()   { return arbolUCI; }
    ColaPrioridad&  cola()    { return colaTriage; }
    Grafo&          grafo()   { return grafoDerivaciones; }
    EventLog&       eventos() { return bitacora; }
    MongoAPI&       mongo()   { return mongoAPI; }
};
