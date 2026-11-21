// ================================================================
//  main.cpp  –  Interfaz de consola del Sistema EHR
//  Plataforma de Salud Digital e Historial Clínico (EHR)
//  Curso: Estructuras de Datos II
// ================================================================
#include <iostream>
#include <string>
#include <limits>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include "../include/ApiRouter.h"   // incluye EHRManager.h y el servidor HTTP

// ── Utilidades de consola ─────────────────────────────────────────
static void limpiarBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

static void pausar() {
    std::cout << "\n  Presione Enter para continuar...";
    limpiarBuffer();
}

static void encabezado(const std::string& titulo) {
    std::cout << "\n  ╔══════════════════════════════════════════╗\n";
    std::cout << "  ║  " << std::left << std::setw(41) << titulo << "║\n";
    std::cout << "  ╚══════════════════════════════════════════╝\n\n";
}

static int leerEntero(const std::string& prompt, int min, int max) {
    int val;
    while (true) {
        std::cout << "  " << prompt;
        if (std::cin >> val && val >= min && val <= max) {
            limpiarBuffer();
            return val;
        }
        std::cin.clear();
        limpiarBuffer();
        std::cout << "  ⚠  Ingrese un valor entre " << min << " y " << max << ".\n";
    }
}

static std::string leerLinea(const std::string& prompt) {
    std::cout << "  " << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

// Lee un numero; Enter vacio = valor normal (0 puntos SOFA)
static double leerDoubleDef(const std::string& prompt, double def) {
    while (true) {
        std::ostringstream d; d << def;
        std::string s = leerLinea(prompt + " [" + d.str() + "] ");
        if (s.empty()) return def;
        try { return std::stod(s); } catch (...) { std::cout << "  ⚠  Valor inválido.\n"; }
    }
}

// Pide los parametros clinicos y calcula el SOFA automaticamente.
// Enter en cada campo = valor normal (0 puntos).
static ParametrosSOFA leerParametrosSOFA() {
    std::cout << "\n  ── Parámetros clínicos (SOFA automático) ──\n"
              << "  Enter = valor normal. El score SOFA se calcula solo.\n\n";
    ParametrosSOFA p;
    p.medido      = true;
    p.pao2fio2    = leerDoubleDef("PaO2/FiO2 (mmHg, normal 400)       ", 400);
    if (p.pao2fio2 < 200) {
        std::string v = leerLinea("¿Ventilación mecánica? (s/n)            ");
        p.ventilado = (v == "s" || v == "S");
    }
    p.plaquetas   = leerDoubleDef("Plaquetas (x10^3/uL, normal 200)     ", 200);
    p.bilirrubina = leerDoubleDef("Bilirrubina (mg/dL, normal 0.5)      ", 0.5);
    p.pam         = leerDoubleDef("Presión arterial media (normal 80)   ", 80);
    p.vasopresor  = (int)leerDoubleDef("Vasopresor 0=ninguno 1=dopa<=5 2=dopa>5 3=dopa>15", 0);
    p.glasgow     = (int)leerDoubleDef("Glasgow (3-15, normal 15)         ", 15);
    p.creatinina  = leerDoubleDef("Creatinina (mg/dL, normal 0.8)       ", 0.8);
    p.vasopresor  = std::max(0, std::min(3, p.vasopresor));
    p.glasgow     = std::max(3, std::min(15, p.glasgow));

    ComponentesSOFA c = SOFA::calcular(p);
    std::cout << "\n  SOFA calculado = " << c.total() << "  (resp " << c.respiratorio
              << " | coag " << c.coagulacion << " | hepát " << c.hepatico
              << " | cardio " << c.cardiovascular << " | SNC " << c.neurologico
              << " | renal " << c.renal << ")\n";
    return p;
}

// ── Sub-menú: Insertar paciente ───────────────────────────────────
static void menuInsertar(EHRManager& ehr) {
    encabezado("INSERTAR NUEVO PACIENTE");

    Paciente p;
    p.idPaciente  = leerLinea("ID Paciente (DNI/QR)   : ");
    p.nombre      = leerLinea("Nombre completo        : ");
    p.tipoSangre  = leerLinea("Tipo de sangre (ej A+) : ");
    p.edad        = leerEntero("Edad (0-130)           : ", 0, 130);
    p.diagnostico = leerLinea("Diagnóstico (opcional)  : ");
    p.nivelTriage = leerEntero("Nivel Triage (1=crítico, 5=leve): ", 1, 5);
    p.sofaParams  = leerParametrosSOFA();
    p.recalcularSOFA();

    std::string uci = leerLinea("¿Está en UCI? (s/n)   : ");
    p.enUCI = (uci == "s" || uci == "S");

    std::string alergiasStr = leerLinea("Alergias (separar con ; o vacío): ");
    if (!alergiasStr.empty()) {
        std::istringstream ss(alergiasStr);
        std::string a;
        while (std::getline(ss, a, ';'))
            if (!a.empty()) p.alergias.push_back(a);
    }

    if (ehr.insertar(p))
        std::cout << "\n  ✔ Paciente registrado exitosamente.\n";
    pausar();
}

// ── Sub-menú: Buscar paciente ─────────────────────────────────────
static void menuBuscar(EHRManager& ehr) {
    encabezado("BUSCAR PACIENTE  [O(1) – Hash Dinámico]");
    std::string id = leerLinea("ID Paciente: ");

    Paciente* p = ehr.buscarPorId(id);
    if (p) {
        std::cout << "\n" << p->toString();
    } else {
        std::cout << "\n  ✘ Paciente no encontrado.\n";
    }
    pausar();
}

// ── Sub-menú: Eliminar paciente ───────────────────────────────────
static void menuEliminar(EHRManager& ehr) {
    encabezado("ELIMINAR PACIENTE");
    std::string id = leerLinea("ID Paciente a eliminar: ");

    // Confirmar
    Paciente* p = ehr.buscarPorId(id);
    if (!p) {
        std::cout << "\n  ✘ Paciente no encontrado.\n";
        pausar();
        return;
    }
    std::cout << "\n  Paciente: " << p->nombre << " (ID=" << p->idPaciente << ")\n";
    std::string conf = leerLinea("¿Confirmar eliminación? (s/n): ");
    if (conf == "s" || conf == "S") {
        ehr.eliminar(id);
        std::cout << "  ✔ Eliminado.\n";
    } else {
        std::cout << "  Operación cancelada.\n";
    }
    pausar();
}

// ── Sub-menú: Actualizar gravedad ─────────────────────────────────
static void menuActualizarGravedad(EHRManager& ehr) {
    encabezado("ACTUALIZAR GRAVEDAD DE PACIENTE");
    std::string id = leerLinea("ID Paciente: ");

    Paciente* p = ehr.buscarPorId(id);
    if (!p) {
        std::cout << "\n  ✘ Paciente no encontrado.\n";
        pausar();
        return;
    }
    std::cout << "\n  Actual → Triage: " << p->nivelTriage
              << " | SOFA: " << p->scoreSOFA
              << " | UCI: " << (p->enUCI ? "Sí" : "No") << "\n\n";

    int    nuevoTriage = leerEntero("Nuevo nivel Triage (1–5): ", 1, 5);
    ParametrosSOFA params = leerParametrosSOFA();
    std::string uci    = leerLinea("¿Pasa a UCI? (s/n)       : ");
    bool   pasaUCI     = (uci == "s" || uci == "S");

    ehr.actualizarGravedad(id, nuevoTriage, params, pasaUCI);
    std::cout << "  ✔ Gravedad actualizada. Estructuras re-balanceadas.\n";
    pausar();
}

// ── Sub-menú: Despachar Triage ────────────────────────────────────
static void menuDespacharTriage(EHRManager& ehr) {
    encabezado("DESPACHAR PACIENTE MÁS URGENTE  [O(1) – Max-Heap]");
    try {
        const Paciente& prox = ehr.proximoTriage();
        std::cout << "  Próximo a atender:\n" << prox.toString();

        std::string conf = leerLinea("¿Confirmar despacho? (s/n): ");
        if (conf == "s" || conf == "S") {
            Paciente despachado = ehr.despacharTriage();
            std::cout << "\n  ✔ Despachado: " << despachado.nombre
                      << " (Triage " << despachado.nivelTriage << ")\n";
        } else {
            std::cout << "  Operación cancelada.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "  ✘ " << e.what() << "\n";
    }
    pausar();
}

// ── Sub-menú: Ranking UCI ─────────────────────────────────────────
static void menuRankingUCI(EHRManager& ehr) {
    encabezado("RANKING UCI  [O(log n) – Árbol Rojo-Negro]");
    auto lista = ehr.rankingUCI();

    if (lista.empty()) {
        std::cout << "  No hay pacientes en UCI.\n";
        pausar();
        return;
    }

    std::cout << "  Pacientes UCI ordenados por gravedad (SOFA desc):\n\n";
    std::cout << "  ┌─────┬──────────────┬────────────────────┬──────────┬────────┐\n";
    std::cout << "  │ Pos │ ID           │ Nombre             │ SOFA     │ Triage │\n";
    std::cout << "  ├─────┼──────────────┼────────────────────┼──────────┼────────┤\n";
    int pos = 1;
    for (const auto& p : lista) {
        printf("  │ %-3d │ %-12s │ %-18s │ %-8.2f │   %-4d │\n",
               pos++, p.idPaciente.c_str(), p.nombre.c_str(),
               p.scoreSOFA, p.nivelTriage);
    }
    std::cout << "  └─────┴──────────────┴────────────────────┴──────────┴────────┘\n";

    std::cout << "\n  Árbol Rojo-Negro (estructura interna):\n";
    ehr.mostrarArbolUCI();
    pausar();
}

// ── Sub-menú: Listar todos los pacientes ─────────────────────────
static void menuListarTodos(EHRManager& ehr) {
    encabezado("TODOS LOS PACIENTES  [Hash Dinámico]");
    auto lista = ehr.listarTodos();

    if (lista.empty()) {
        std::cout << "  No hay pacientes registrados.\n";
        pausar();
        return;
    }

    std::cout << "  Total: " << lista.size() << " paciente(s)\n\n";
    std::cout << "  ┌──────────────┬────────────────────┬────────┬──────────┬──────┬────┐\n";
    std::cout << "  │ ID           │ Nombre             │ Sangre │ SOFA     │ T    │ UCI│\n";
    std::cout << "  ├──────────────┼────────────────────┼────────┼──────────┼──────┼────┤\n";
    for (const auto& p : lista) {
        printf("  │ %-12s │ %-18s │ %-6s │ %-8.2f │  %-3d │ %-2s │\n",
               p.idPaciente.c_str(), p.nombre.c_str(), p.tipoSangre.c_str(),
               p.scoreSOFA, p.nivelTriage, p.enUCI ? "Sí" : "No");
    }
    std::cout << "  └──────────────┴────────────────────┴────────┴──────────┴──────┴────┘\n";
    pausar();
}

// ── Sub-menú: Visualizar estructuras internas ─────────────────────
static void menuVisualizarEstructuras(EHRManager& ehr) {
    while (true) {
        encabezado("VISUALIZAR ESTRUCTURAS INTERNAS");
        std::cout << "  [1] Tabla Hash Dinámica (distribución de buckets)\n";
        std::cout << "  [2] Árbol Rojo-Negro UCI\n";
        std::cout << "  [3] Cola de Prioridad Triage (Max-Heap)\n";
        std::cout << "  [4] Estadísticas generales\n";
        std::cout << "  [0] Regresar\n\n";

        int op = leerEntero("Opción: ", 0, 4);
        switch (op) {
            case 1: encabezado("TABLA HASH"); ehr.mostrarHash();       pausar(); break;
            case 2: encabezado("ÁRBOL RBT");  ehr.mostrarArbolUCI();   pausar(); break;
            case 3: encabezado("MAX-HEAP");   ehr.mostrarColaTriage(); pausar(); break;
            case 4: ehr.mostrarEstadisticas(); pausar(); break;
            case 0: return;
        }
    }
}

// ── Menú principal ────────────────────────────────────────────────
static void menuPrincipal(EHRManager& ehr) {
    while (true) {
        std::cout << "\n";
        std::cout << "  ╔══════════════════════════════════════════╗\n";
        std::cout << "  ║         MENÚ PRINCIPAL – EHR             ║\n";
        std::cout << "  ╠══════════════════════════════════════════╣\n";
        std::cout << "  ║  [1] Insertar paciente                   ║\n";
        std::cout << "  ║  [2] Buscar paciente por ID    [O(1)]    ║\n";
        std::cout << "  ║  [3] Eliminar paciente                   ║\n";
        std::cout << "  ║  [4] Actualizar gravedad                 ║\n";
        std::cout << "  ║  [5] Despachar Triage más urgente [O(1)] ║\n";
        std::cout << "  ║  [6] Ranking UCI por gravedad  [O(log n)]║\n";
        std::cout << "  ║  [7] Listar todos los pacientes          ║\n";
        std::cout << "  ║  [8] Visualizar estructuras internas     ║\n";
        std::cout << "  ║  [0] Salir                               ║\n";
        std::cout << "  ╚══════════════════════════════════════════╝\n\n";

        int op = leerEntero("Seleccione opción: ", 0, 8);

        switch (op) {
            case 1: menuInsertar(ehr);              break;
            case 2: menuBuscar(ehr);                break;
            case 3: menuEliminar(ehr);              break;
            case 4: menuActualizarGravedad(ehr);    break;
            case 5: menuDespacharTriage(ehr);       break;
            case 6: menuRankingUCI(ehr);            break;
            case 7: menuListarTodos(ehr);           break;
            case 8: menuVisualizarEstructuras(ehr); break;
            case 0:
                std::cout << "\n  Cerrando sistema EHR. ¡Hasta pronto!\n\n";
                return;
        }
    }
}

// ── Punto de entrada ──────────────────────────────────────────────
//  Modo consola (por defecto):  ehr_sistema.exe
//  Modo web:  ehr_sistema.exe --server [--port 8080] [--web ruta\web]
//             y abrir http://127.0.0.1:8080
static bool carpetaWebValida(const std::string& dir) {
    std::ifstream f(dir + "/index.html");
    return f.good();
}

static int modoServidor(EHRManager& ehr, int puerto, std::string webDir) {
    if (webDir.empty()) {
        // Buscar la carpeta web/ junto al proyecto, ejecutando desde build\ o desde la raiz
        const char* candidatos[] = {"web", "../web", "../../web"};
        for (const char* c : candidatos)
            if (carpetaWebValida(c)) { webDir = c; break; }
    }
    if (webDir.empty() || !carpetaWebValida(webDir)) {
        std::cout << "  ✘ No se encontró la carpeta web/ (index.html). Use --web <ruta>.\n";
        return 1;
    }

    ApiRouter router(ehr);
    HttpServer server(puerto, webDir, [&router](const HttpRequest& r) {
        return router.manejar(r);
    });
    if (!server.iniciar()) {
        std::cout << "  ✘ No se pudo abrir el puerto " << puerto
                  << " (¿ya está en uso?). Pruebe con --port <otro>.\n";
        return 1;
    }
    std::cout << "  ╔══════════════════════════════════════════════╗\n"
              << "  ║  Servidor web activo                         ║\n"
              << "  ║  Abra:  http://127.0.0.1:" << std::left << std::setw(20) << puerto << "║\n"
              << "  ║  Ctrl+C para detener                         ║\n"
              << "  ╚══════════════════════════════════════════════╝\n"
              << "  Carpeta web: " << webDir << "\n\n";
    server.correr();
    return 0;
}

int main(int argc, char** argv) {
    // Configurar consola UTF-8 en Windows
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif

    bool servidor = false;
    int  puerto   = 8080;
    std::string webDir;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--server") servidor = true;
        else if (a == "--port" && i + 1 < argc) {
            try { puerto = std::stoi(argv[++i]); } catch (...) { puerto = 8080; }
        }
        else if (a == "--web" && i + 1 < argc) webDir = argv[++i];
    }

    EHRManager ehr(false);  // false = modo silencioso (sin logs curl)
    ehr.inicializar();

    if (servidor) return modoServidor(ehr, puerto, webDir);

    menuPrincipal(ehr);
    return 0;
}
