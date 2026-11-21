#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include "HttpServer.h"    // winsock2 antes que windows.h (via MongoAPI.h)
#include "EHRManager.h"
#include "Serializador.h"

// ================================================================
//  ApiRouter.h  –  API REST JSON del sistema EHR
//
//  Todas las respuestas: {"ok":true|false, ...}. Errores: {"ok":false,"error":"..."}
//  Las operaciones de escritura incluyen "trace": que hizo cada
//  estructura (bucket/split del hash, rotaciones del arbol, movimientos
//  del heap) para que el visualizador pueda animarlas.
//
//  Rutas (ver .kiro/specs/ehr-frontend/design.md):
//    GET    /api/estado
//    POST   /api/nube/reconectar
//    GET    /api/pacientes[?q=texto]
//    GET    /api/pacientes/{id}?via=hash|arbol[&sofa=x]
//    POST   /api/pacientes          PUT/DELETE /api/pacientes/{id}
//    POST   /api/sofa/calcular
//    GET    /api/triage             POST /api/triage/despachar
//    GET    /api/estructuras/{arbol|hash|heap}
//    GET    /api/grafo   POST /api/grafo/nodos   DELETE /api/grafo/nodos/{id}
//    POST   /api/grafo/aristas   DELETE /api/grafo/aristas?de=&a=
//    GET    /api/grafo/ruta?de=&a=   GET /api/grafo/alcanzables?de=
//    POST   /api/grafo/derivar
//    GET    /api/eventos?desde=N
// ================================================================

class ApiRouter {
private:
    EHRManager& ehr;
    std::chrono::steady_clock::time_point inicio;

    struct Instantanea { int splits; long rot; long inter; int uci; };

    // ── Respuestas ───────────────────────────────────────────────
    static HttpResponse json(int estado, const std::string& cuerpo) {
        HttpResponse r; r.estado = estado; r.cuerpo = cuerpo; return r;
    }
    static HttpResponse ok(const std::string& campos = "", int estado = 200) {
        return json(estado, "{\"ok\":true" + (campos.empty() ? "" : "," + campos) + "}");
    }
    static HttpResponse error(int estado, const std::string& msg) {
        return json(estado, "{\"ok\":false,\"error\":" + Out::q(msg) + "}");
    }

    // ── Parseo del cuerpo JSON ───────────────────────────────────
    static bool tiene(const std::string& j, const std::string& k) {
        return j.find("\"" + k + "\":") != std::string::npos;
    }
    static bool numero(const std::string& j, const std::string& k, double& out) {
        std::string s = JSON::getNumber(j, k);
        if (s.empty()) return false;
        try { out = std::stod(s); return true; } catch (...) { return false; }
    }
    static std::vector<std::string> arreglo(const std::string& j, const std::string& k) {
        std::vector<std::string> r;
        auto p = j.find("\"" + k + "\":[");
        if (p == std::string::npos) return r;
        p += k.size() + 4;
        auto e = j.find(']', p);
        std::string a = j.substr(p, e == std::string::npos ? std::string::npos : e - p);
        size_t i = 0;
        while (i < a.size()) {
            if (a[i] == '"') {
                std::string v; i++;
                while (i < a.size() && a[i] != '"') {
                    if (a[i] == '\\' && i + 1 < a.size()) i++;
                    v += a[i]; i++;
                }
                if (!v.empty()) r.push_back(v);
            }
            i++;
        }
        return r;
    }

    // Lee los parametros SOFA presentes en 'j' sobre 'ps'; true si habia alguno
    static bool leerParametrosSOFA(const std::string& j, ParametrosSOFA& ps) {
        bool alguno = false;
        double v;
        if (numero(j, "pao2fio2",    v)) { ps.pao2fio2    = v; alguno = true; }
        if (numero(j, "plaquetas",   v)) { ps.plaquetas   = v; alguno = true; }
        if (numero(j, "bilirrubina", v)) { ps.bilirrubina = v; alguno = true; }
        if (numero(j, "pam",         v)) { ps.pam         = v; alguno = true; }
        if (numero(j, "vasopresor",  v)) { ps.vasopresor  = (int)v; alguno = true; }
        if (numero(j, "glasgow",     v)) { ps.glasgow     = (int)v; alguno = true; }
        if (numero(j, "creatinina",  v)) { ps.creatinina  = v; alguno = true; }
        if (tiene(j, "ventilado"))       { ps.ventilado   = JSON::getBool(j, "ventilado"); alguno = true; }
        return alguno;
    }

    // Valida rangos; devuelve "" si todo bien
    static std::string validarSOFA(const ParametrosSOFA& p) {
        if (p.pao2fio2 <= 0 || p.pao2fio2 > 700) return "PaO2/FiO2 debe estar entre 1 y 700";
        if (p.plaquetas < 0 || p.plaquetas > 1500) return "Plaquetas fuera de rango (0-1500)";
        if (p.bilirrubina < 0 || p.bilirrubina > 60) return "Bilirrubina fuera de rango (0-60)";
        if (p.pam < 0 || p.pam > 200) return "PAM fuera de rango (0-200)";
        if (p.vasopresor < 0 || p.vasopresor > 3) return "Vasopresor debe ser 0-3";
        if (p.glasgow < 3 || p.glasgow > 15) return "Glasgow debe estar entre 3 y 15";
        if (p.creatinina < 0 || p.creatinina > 30) return "Creatinina fuera de rango (0-30)";
        return "";
    }

    // Aplica los campos presentes en el cuerpo sobre 'p'. Devuelve "" o mensaje de error.
    static std::string aplicarCuerpo(const std::string& cuerpo, Paciente& p) {
        std::string j = JSON::aplanar(cuerpo);
        if (j.empty() || j[0] != '{') return "Cuerpo JSON invalido";

        if (tiene(j, "idPaciente"))  p.idPaciente  = JSON::getString(j, "idPaciente");
        if (tiene(j, "nombre"))      p.nombre      = JSON::getString(j, "nombre");
        if (tiene(j, "tipoSangre"))  p.tipoSangre  = JSON::getString(j, "tipoSangre");
        if (tiene(j, "diagnostico")) p.diagnostico = JSON::getString(j, "diagnostico");
        if (tiene(j, "alergias"))    p.alergias    = arreglo(j, "alergias");
        if (tiene(j, "enUCI"))       p.enUCI       = JSON::getBool(j, "enUCI");

        double v;
        if (numero(j, "edad", v)) {
            if (v < 0 || v > 130) return "Edad debe estar entre 0 y 130";
            p.edad = (int)v;
        }
        if (numero(j, "nivelTriage", v)) {
            if (v < 1 || v > 5) return "nivelTriage debe estar entre 1 y 5";
            p.nivelTriage = (int)v;
        }

        bool hayParams = leerParametrosSOFA(j, p.sofaParams);
        if (tiene(j, "sofaMedido")) p.sofaParams.medido = JSON::getBool(j, "sofaMedido");
        else if (hayParams)         p.sofaParams.medido = true;

        if (p.sofaParams.medido) {
            std::string e = validarSOFA(p.sofaParams);
            if (!e.empty()) return e;
        } else if (numero(j, "scoreSOFA", v)) {
            if (v < 0 || v > 24) return "scoreSOFA debe estar entre 0 y 24";
            p.scoreSOFA = v;   // SOFA manual (solo registros sin parametros)
        }
        return "";
    }

    // ── Instantaneas para el "trace" ─────────────────────────────
    Instantanea tomar() {
        return { ehr.hash().splits(), ehr.arbol().rotaciones(),
                 ehr.cola().intercambios(), ehr.arbol().tamanio() };
    }

    std::string trace(const Instantanea& a, const std::string& id) {
        Instantanea b = tomar();
        std::ostringstream o;
        o << "{\"hash\":{\"bucket\":" << ehr.hash().bucketDe(id)
          << ",\"split\":" << Out::boolean(b.splits > a.splits)
          << ",\"splitsNuevos\":" << (b.splits - a.splits)
          << ",\"buckets\":" << ehr.hash().buckets()
          << ",\"elementos\":" << ehr.hash().elementos()
          << ",\"carga\":" << Out::num(ehr.hash().carga()) << "}"
          << ",\"arbol\":{\"rotaciones\":" << (b.rot - a.rot)
          << ",\"tamanioAntes\":" << a.uci << ",\"tamanio\":" << b.uci << "}"
          << ",\"heap\":{\"intercambios\":" << (b.inter - a.inter)
          << ",\"tamanio\":" << ehr.cola().tamanio()
          << ",\"movimientos\":" << ehr.cola().movimientosJson() << "}}";
        return o.str();
    }

    static std::vector<std::string> segmentos(const std::string& ruta) {
        std::vector<std::string> s;
        std::istringstream ss(ruta);
        std::string t;
        while (std::getline(ss, t, '/')) if (!t.empty()) s.push_back(t);
        return s;
    }

    static std::string param(const HttpRequest& r, const std::string& k) {
        auto it = r.query.find(k);
        return it == r.query.end() ? "" : it->second;
    }

    static std::string minus(std::string s) {
        for (auto& c : s) c = (char)tolower((unsigned char)c);
        return s;
    }

    // ── Handlers ─────────────────────────────────────────────────
    HttpResponse estado() {
        long seg = (long)std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - inicio).count();
        std::ostringstream o;
        o << "\"backend\":\"online\""
          << ",\"nube\":" << Out::boolean(ehr.nubeEstaActiva())
          << ",\"ultimoSyncOk\":" << Out::boolean(ehr.ultimoSincronizadoOk())
          << ",\"pacientes\":" << ehr.hash().elementos()
          << ",\"uci\":" << ehr.arbol().tamanio()
          << ",\"triage\":" << ehr.cola().tamanio()
          << ",\"hash\":{\"buckets\":" << ehr.hash().buckets()
          << ",\"carga\":" << Out::num(ehr.hash().carga()) << "}"
          << ",\"grafo\":{\"nodos\":" << ehr.grafo().numNodos()
          << ",\"aristas\":" << ehr.grafo().numAristas() << "}"
          << ",\"uptimeSeg\":" << seg
          << ",\"ultimoEvento\":" << ehr.eventos().ultimoId();
        return ok(o.str());
    }

    HttpResponse listar(const HttpRequest& req) {
        std::string q = minus(param(req, "q"));
        auto lista = ehr.listarTodos();
        std::sort(lista.begin(), lista.end(),
                  [](const Paciente& a, const Paciente& b) { return a.idPaciente < b.idPaciente; });
        std::string s = "[";
        bool primero = true;
        for (const auto& p : lista) {
            if (!q.empty() && minus(p.idPaciente).find(q) == std::string::npos &&
                minus(p.nombre).find(q) == std::string::npos &&
                minus(p.diagnostico).find(q) == std::string::npos) continue;
            if (!primero) s += ",";
            primero = false;
            s += Out::pacienteJson(p);
        }
        s += "]";
        return ok("\"total\":" + std::to_string(ehr.hash().elementos()) + ",\"pacientes\":" + s);
    }

    HttpResponse buscar(const HttpRequest& req, const std::string& id) {
        std::string via = param(req, "via");
        if (via.empty()) via = "hash";
        if (via != "hash" && via != "arbol") return error(400, "via debe ser 'hash' o 'arbol'");

        using clk = std::chrono::high_resolution_clock;
        std::ostringstream o;
        Paciente* p = nullptr;
        std::string pasos = "[";
        std::string complejidad, nota;
        int comps = 0;
        double us = 0;

        if (via == "hash") {
            int bucket = 0;
            auto t0 = clk::now();
            p = ehr.hash().buscarConTraza(id, bucket, comps);
            us = std::chrono::duration<double, std::micro>(clk::now() - t0).count();
            complejidad = "O(1) promedio";
            pasos += Out::q("FNV-1a(\"" + id + "\") mod 2^nivel -> bucket " + std::to_string(bucket));
            pasos += "," + Out::q("Recorrer cadena del bucket: " + std::to_string(comps) + " comparacion(es)");
            o << "\"bucket\":" << bucket << ",";
        } else {
            std::vector<std::string> camino;
            std::string sofaStr = param(req, "sofa");
            auto t0 = clk::now();
            if (!sofaStr.empty()) {
                double sofa = 0;
                try { sofa = std::stod(sofaStr); } catch (...) { return error(400, "sofa invalido"); }
                p = ehr.arbol().buscarPorClave(sofa, id, &camino, &comps);
                complejidad = "O(log n) por clave (SOFA, id)";
            } else {
                p = ehr.arbol().buscarPorIdConTraza(id, camino, comps);
                complejidad = "O(n): el arbol se ordena por SOFA, buscar solo por id recorre en preorden";
            }
            us = std::chrono::duration<double, std::micro>(clk::now() - t0).count();
            for (size_t i = 0; i < camino.size(); i++) {
                if (i) pasos += ",";
                pasos += Out::q("Visita nodo " + camino[i]);
            }
            o << "\"camino\":[";
            for (size_t i = 0; i < camino.size(); i++) { if (i) o << ","; o << Out::q(camino[i]); }
            o << "],";
            if (!p) {
                Paciente* enHash = ehr.hash().buscar(id);
                if (enHash && !enHash->enUCI)
                    nota = "El paciente existe pero no esta en UCI: el arbol solo indexa pacientes UCI.";
                else if (enHash && !sofaStr.empty())
                    nota = "El paciente esta en UCI pero la clave (SOFA, id) no coincide; su SOFA actual es "
                           + Out::num(enHash->scoreSOFA) + ".";
            }
        }
        pasos += "]";

        o << "\"via\":" << Out::q(via)
          << ",\"encontrado\":" << Out::boolean(p != nullptr)
          << ",\"complejidad\":" << Out::q(complejidad)
          << ",\"comparaciones\":" << comps
          << ",\"microsegundos\":" << Out::num(us)
          << ",\"pasos\":" << pasos
          << ",\"nota\":" << Out::q(nota)
          << ",\"paciente\":" << (p ? Out::pacienteJson(*p) : "null");
        return ok(o.str());
    }

    HttpResponse crear(const HttpRequest& req) {
        Paciente p;
        std::string e = aplicarCuerpo(req.cuerpo, p);
        if (!e.empty()) return error(400, e);
        if (p.idPaciente.empty()) return error(400, "idPaciente es obligatorio");
        if (p.nombre.empty())     return error(400, "nombre es obligatorio");
        if (ehr.buscarPorId(p.idPaciente)) return error(409, "Ya existe un paciente con ID " + p.idPaciente);

        Instantanea antes = tomar();
        if (!ehr.insertar(p)) return error(400, "No se pudo insertar el paciente");
        Paciente* ins = ehr.buscarPorId(p.idPaciente);
        return ok("\"mensaje\":\"Paciente insertado\",\"paciente\":" + Out::pacienteJson(*ins)
                  + ",\"trace\":" + trace(antes, p.idPaciente), 201);
    }

    HttpResponse actualizar(const HttpRequest& req, const std::string& id) {
        Paciente* actual = ehr.buscarPorId(id);
        if (!actual) return error(404, "No existe el paciente " + id);
        Paciente nuevo = *actual;
        std::string e = aplicarCuerpo(req.cuerpo, nuevo);
        if (!e.empty()) return error(400, e);
        if (nuevo.nombre.empty()) return error(400, "nombre no puede estar vacio");

        Instantanea antes = tomar();
        if (!ehr.actualizar(id, nuevo)) return error(500, "No se pudo actualizar");
        Paciente* act = ehr.buscarPorId(id);
        return ok("\"mensaje\":\"Paciente actualizado\",\"paciente\":" + Out::pacienteJson(*act)
                  + ",\"trace\":" + trace(antes, id));
    }

    HttpResponse eliminar(const std::string& id) {
        if (!ehr.buscarPorId(id)) return error(404, "No existe el paciente " + id);
        Instantanea antes = tomar();
        ehr.eliminar(id);
        return ok("\"mensaje\":\"Paciente eliminado\",\"trace\":" + trace(antes, id));
    }

    HttpResponse calcularSOFA(const HttpRequest& req) {
        std::string j = JSON::aplanar(req.cuerpo);
        ParametrosSOFA ps;
        leerParametrosSOFA(j, ps);
        std::string e = validarSOFA(ps);
        if (!e.empty()) return error(400, e);
        ComponentesSOFA c = SOFA::calcular(ps);
        std::ostringstream o;
        o << "\"total\":" << c.total()
          << ",\"componentes\":{\"respiratorio\":" << c.respiratorio
          << ",\"coagulacion\":" << c.coagulacion
          << ",\"hepatico\":" << c.hepatico
          << ",\"cardiovascular\":" << c.cardiovascular
          << ",\"neurologico\":" << c.neurologico
          << ",\"renal\":" << c.renal << "}";
        return ok(o.str());
    }

    HttpResponse triage() {
        std::string sig = "null";
        if (!ehr.colaVacia()) sig = Out::pacienteJson(ehr.proximoTriage());
        return ok("\"cola\":" + ehr.cola().exportarJson() + ",\"siguiente\":" + sig);
    }

    HttpResponse despacharPaciente() {
        if (ehr.colaVacia()) return error(409, "La cola de triaje esta vacia");
        Instantanea antes = tomar();
        Paciente p = ehr.despacharTriage();
        return ok("\"mensaje\":\"Paciente despachado y dado de alta de las estructuras\""
                  ",\"paciente\":" + Out::pacienteJson(p)
                  + ",\"trace\":" + trace(antes, p.idPaciente));
    }

    // ── Grafo ────────────────────────────────────────────────────
    HttpResponse grafo(const HttpRequest& req, const std::vector<std::string>& seg) {
        Grafo& g = ehr.grafo();
        const std::string& m = req.metodo;
        std::string j = JSON::aplanar(req.cuerpo);

        if (seg.size() == 2 && m == "GET")
            return ok("\"grafo\":" + g.exportarJson());

        if (seg.size() >= 3 && seg[2] == "nodos") {
            if (m == "POST" && seg.size() == 3) {
                NodoGrafo n;
                n.id = JSON::getString(j, "id");
                n.nombre = JSON::getString(j, "nombre");
                n.tipo = JSON::getString(j, "tipo");
                if (n.id.empty() || n.nombre.empty()) return error(400, "id y nombre son obligatorios");
                if (n.tipo != "hospital" && n.tipo != "centro" && n.tipo != "especialista")
                    return error(400, "tipo debe ser hospital, centro o especialista");
                if (!g.agregarNodo(n)) return error(409, "Ya existe el nodo " + n.id);
                return ok("\"grafo\":" + g.exportarJson(), 201);
            }
            if (m == "DELETE" && seg.size() == 4) {
                if (!g.eliminarNodo(seg[3])) return error(404, "No existe el nodo " + seg[3]);
                return ok("\"grafo\":" + g.exportarJson());
            }
        }
        if (seg.size() == 3 && seg[2] == "aristas") {
            if (m == "POST") {
                std::string de = JSON::getString(j, "de"), a = JSON::getString(j, "a");
                double min = 0;
                if (!numero(j, "minutos", min) || min < 0) return error(400, "minutos debe ser un numero >= 0");
                if (!g.existe(de) || !g.existe(a)) return error(404, "Origen o destino inexistente");
                if (de == a) return error(400, "Origen y destino deben ser distintos");
                g.agregarArista(de, a, (int)min, JSON::getString(j, "motivo"));
                return ok("\"grafo\":" + g.exportarJson(), 201);
            }
            if (m == "DELETE") {
                if (!g.eliminarArista(param(req, "de"), param(req, "a")))
                    return error(404, "No existe esa derivacion");
                return ok("\"grafo\":" + g.exportarJson());
            }
        }
        if (seg.size() == 3 && seg[2] == "ruta" && m == "GET") {
            std::vector<std::string> camino;
            int min = 0;
            std::string de = param(req, "de"), a = param(req, "a");
            if (!g.existe(de) || !g.existe(a)) return error(404, "Origen o destino inexistente");
            if (!g.rutaMasCorta(de, a, camino, min)) return error(404, "No hay ruta de derivacion entre esos nodos");
            std::string c = "[";
            for (size_t i = 0; i < camino.size(); i++) { if (i) c += ","; c += Out::q(camino[i]); }
            return ok("\"camino\":" + c + "],\"minutos\":" + std::to_string(min));
        }
        if (seg.size() == 3 && seg[2] == "alcanzables" && m == "GET") {
            std::string de = param(req, "de");
            if (!g.existe(de)) return error(404, "Nodo inexistente");
            auto v = g.alcanzables(de);
            std::string c = "[";
            for (size_t i = 0; i < v.size(); i++) { if (i) c += ","; c += Out::q(v[i]); }
            return ok("\"alcanzables\":" + c + "]");
        }
        if (seg.size() == 3 && seg[2] == "derivar" && m == "POST") {
            std::string id = JSON::getString(j, "idPaciente");
            std::string de = JSON::getString(j, "de"), a = JSON::getString(j, "a");
            if (!ehr.buscarPorId(id)) return error(404, "No existe el paciente " + id);
            int n = g.derivar(id, de, a, JSON::getString(j, "motivo"));
            if (n == 0) return error(400, "No existe una arista de derivacion " + de + " -> " + a);
            return ok("\"numero\":" + std::to_string(n) + ",\"grafo\":" + g.exportarJson(), 201);
        }
        return error(404, "Ruta de grafo no encontrada");
    }

    HttpResponse despachar(const HttpRequest& req) {
        auto seg = segmentos(req.ruta);
        const std::string& m = req.metodo;
        if (seg.size() < 2) return error(404, "Ruta no encontrada");
        const std::string& r = seg[1];

        if (r == "estado" && m == "GET") return estado();

        if (r == "nube" && seg.size() == 3 && seg[2] == "reconectar" && m == "POST") {
            bool c = ehr.reconectarNube();
            return ok("\"nube\":" + Out::boolean(c));
        }

        if (r == "pacientes") {
            if (seg.size() == 2 && m == "GET")  return listar(req);
            if (seg.size() == 2 && m == "POST") return crear(req);
            if (seg.size() == 3 && m == "GET")    return buscar(req, seg[2]);
            if (seg.size() == 3 && m == "PUT")    return actualizar(req, seg[2]);
            if (seg.size() == 3 && m == "DELETE") return eliminar(seg[2]);
        }

        if (r == "sofa" && seg.size() == 3 && seg[2] == "calcular" && m == "POST")
            return calcularSOFA(req);

        if (r == "triage") {
            if (seg.size() == 2 && m == "GET") return triage();
            if (seg.size() == 3 && seg[2] == "despachar" && m == "POST") return despacharPaciente();
        }

        if (r == "estructuras" && seg.size() == 3 && m == "GET") {
            if (seg[2] == "arbol") return ok("\"arbol\":" + ehr.arbol().exportarJson());
            if (seg[2] == "hash")  return ok("\"hash\":"  + ehr.hash().exportarJson());
            if (seg[2] == "heap")  return ok("\"heap\":"  + ehr.cola().exportarJson());
        }

        if (r == "grafo") return grafo(req, seg);

        if (r == "eventos" && m == "GET") {
            long long desde = 0;
            try { desde = std::stoll(param(req, "desde").empty() ? "0" : param(req, "desde")); } catch (...) {}
            std::string e = ehr.eventos().exportarJson(desde);
            return json(200, "{\"ok\":true," + e.substr(1));
        }

        return error(404, "Ruta no encontrada: " + m + " " + req.ruta);
    }

public:
    explicit ApiRouter(EHRManager& e) : ehr(e), inicio(std::chrono::steady_clock::now()) {}

    // Punto de entrada: ejecuta la ruta y registra el evento en la bitacora
    HttpResponse manejar(const HttpRequest& req) {
        auto t0 = std::chrono::steady_clock::now();
        HttpResponse r;
        try { r = despachar(req); }
        catch (const std::exception& e) { r = error(500, e.what()); }
        long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();

        // No registrar el sondeo (ruido)
        if (req.ruta != "/api/eventos" && req.ruta != "/api/estado") {
            auto seg = segmentos(req.ruta);
            std::string estructura = "api";
            if (seg.size() > 1) {
                const std::string& s = seg[1];
                if (s == "pacientes") estructura = "hash";
                else if (s == "triage") estructura = "heap";
                else if (s == "grafo") estructura = "grafo";
                else if (s == "estructuras" && seg.size() > 2) estructura = seg[2];
            }
            Evento e;
            e.tipo = "api"; e.metodo = req.metodo; e.ruta = req.ruta;
            e.estructura = estructura; e.codigo = r.estado; e.ms = ms;
            e.ok = r.estado < 400;
            ehr.eventos().agregar(e);
        }
        return r;
    }
};
