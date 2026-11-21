#pragma once
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <climits>
#include "Serializador.h"

// ================================================================
//  Grafo.h  –  Grafo dirigido y ponderado de derivaciones medicas
//
//  Nodos  : hospitales, centros y especialistas.
//  Aristas: derivacion posible A -> B con tiempo de traslado (min).
//  Representacion: lista de adyacencia  (map id -> vector<arista>).
//
//  Complejidad (V nodos, E aristas):
//    agregarNodo / agregarArista : O(log V + grado)
//    eliminarNodo                : O(V + E)
//    rutaMasCorta (Dijkstra)     : O(V^2)
//    alcanzables  (BFS)          : O(V + E)
//  Los datos viven en memoria (no se persisten en MongoDB).
// ================================================================

struct NodoGrafo {
    std::string id;
    std::string nombre;
    std::string tipo;     // "hospital" | "centro" | "especialista"
};

struct AristaGrafo {
    std::string de, a;
    int         minutos = 0;   // tiempo de traslado
    std::string motivo;
};

struct Derivacion {
    int         numero = 0;
    std::string idPaciente, de, a, motivo;
};

class Grafo {
private:
    std::vector<NodoGrafo>                         nodos;
    std::map<std::string, std::vector<AristaGrafo>> ady;
    std::vector<Derivacion>                        derivaciones;
    int                                            siguienteDerivacion = 1;

    int indice(const std::string& id) const {
        for (size_t i = 0; i < nodos.size(); i++)
            if (nodos[i].id == id) return (int)i;
        return -1;
    }

public:
    Grafo() { cargarEjemplo(); }

    bool existe(const std::string& id) const { return indice(id) >= 0; }

    // ── Nodos ────────────────────────────────────────────────────
    bool agregarNodo(const NodoGrafo& n) {
        if (n.id.empty() || existe(n.id)) return false;
        nodos.push_back(n);
        ady[n.id];
        return true;
    }

    bool eliminarNodo(const std::string& id) {
        int i = indice(id);
        if (i < 0) return false;
        nodos.erase(nodos.begin() + i);
        ady.erase(id);
        for (auto& par : ady) {
            auto& v = par.second;
            v.erase(std::remove_if(v.begin(), v.end(),
                [&](const AristaGrafo& e) { return e.a == id; }), v.end());
        }
        derivaciones.erase(std::remove_if(derivaciones.begin(), derivaciones.end(),
            [&](const Derivacion& d) { return d.de == id || d.a == id; }),
            derivaciones.end());
        return true;
    }

    // ── Aristas (si existe, se actualiza) ────────────────────────
    bool agregarArista(const std::string& de, const std::string& a,
                       int minutos, const std::string& motivo = "") {
        if (!existe(de) || !existe(a) || de == a || minutos < 0) return false;
        for (auto& e : ady[de])
            if (e.a == a) { e.minutos = minutos; e.motivo = motivo; return true; }
        AristaGrafo e; e.de = de; e.a = a; e.minutos = minutos; e.motivo = motivo;
        ady[de].push_back(e);
        return true;
    }

    bool eliminarArista(const std::string& de, const std::string& a) {
        auto it = ady.find(de);
        if (it == ady.end()) return false;
        auto& v = it->second;
        auto antes = v.size();
        v.erase(std::remove_if(v.begin(), v.end(),
            [&](const AristaGrafo& e) { return e.a == a; }), v.end());
        return v.size() != antes;
    }

    const AristaGrafo* arista(const std::string& de, const std::string& a) const {
        auto it = ady.find(de);
        if (it == ady.end()) return nullptr;
        for (const auto& e : it->second) if (e.a == a) return &e;
        return nullptr;
    }

    // ── Derivar un paciente por una arista existente ─────────────
    // Devuelve el numero de derivacion, o 0 si la arista no existe
    int derivar(const std::string& idPaciente, const std::string& de,
                const std::string& a, const std::string& motivo) {
        if (!arista(de, a)) return 0;
        Derivacion d;
        d.numero = siguienteDerivacion++;
        d.idPaciente = idPaciente; d.de = de; d.a = a; d.motivo = motivo;
        derivaciones.push_back(d);
        return d.numero;
    }

    // ── Dijkstra (tiempo minimo de traslado) ─────────────────────
    // Devuelve false si no hay ruta
    bool rutaMasCorta(const std::string& origen, const std::string& destino,
                      std::vector<std::string>& camino, int& minutos) const {
        camino.clear();
        int n = (int)nodos.size();
        int s = indice(origen), t = indice(destino);
        if (s < 0 || t < 0) return false;
        std::vector<long long> dist(n, LLONG_MAX);
        std::vector<int> prev(n, -1);
        std::vector<bool> fijo(n, false);
        dist[s] = 0;
        for (int it = 0; it < n; it++) {
            int u = -1;
            for (int i = 0; i < n; i++)
                if (!fijo[i] && dist[i] != LLONG_MAX && (u < 0 || dist[i] < dist[u])) u = i;
            if (u < 0) break;
            fijo[u] = true;
            auto f = ady.find(nodos[u].id);
            if (f == ady.end()) continue;
            for (const auto& e : f->second) {
                int v = indice(e.a);
                if (v >= 0 && dist[u] + e.minutos < dist[v]) {
                    dist[v] = dist[u] + e.minutos;
                    prev[v] = u;
                }
            }
        }
        if (dist[t] == LLONG_MAX) return false;
        for (int v = t; v >= 0; v = prev[v]) camino.push_back(nodos[v].id);
        std::reverse(camino.begin(), camino.end());
        minutos = (int)dist[t];
        return true;
    }

    // ── BFS: nodos alcanzables desde 'origen' (en orden de visita) ──
    std::vector<std::string> alcanzables(const std::string& origen) const {
        std::vector<std::string> orden;
        if (!existe(origen)) return orden;
        std::vector<std::string> cola{origen};
        std::map<std::string, bool> visto{{origen, true}};
        for (size_t h = 0; h < cola.size(); h++) {
            orden.push_back(cola[h]);
            auto f = ady.find(cola[h]);
            if (f == ady.end()) continue;
            for (const auto& e : f->second)
                if (!visto[e.a]) { visto[e.a] = true; cola.push_back(e.a); }
        }
        return orden;
    }

    int numNodos() const { return (int)nodos.size(); }
    int numAristas() const {
        int c = 0;
        for (const auto& p : ady) c += (int)p.second.size();
        return c;
    }

    // ── Exportacion JSON ─────────────────────────────────────────
    std::string exportarJson() const {
        std::string s = "{\"nodos\":[";
        for (size_t i = 0; i < nodos.size(); i++) {
            if (i) s += ",";
            s += "{\"id\":" + Out::q(nodos[i].id) + ",\"nombre\":" + Out::q(nodos[i].nombre)
               + ",\"tipo\":" + Out::q(nodos[i].tipo) + "}";
        }
        s += "],\"aristas\":[";
        bool primero = true;
        for (const auto& n : nodos) {
            auto f = ady.find(n.id);
            if (f == ady.end()) continue;
            for (const auto& e : f->second) {
                if (!primero) s += ",";
                primero = false;
                s += "{\"de\":" + Out::q(e.de) + ",\"a\":" + Out::q(e.a)
                   + ",\"minutos\":" + std::to_string(e.minutos)
                   + ",\"motivo\":" + Out::q(e.motivo) + "}";
            }
        }
        s += "],\"derivaciones\":[";
        for (size_t i = 0; i < derivaciones.size(); i++) {
            const auto& d = derivaciones[i];
            if (i) s += ",";
            s += "{\"numero\":" + std::to_string(d.numero)
               + ",\"idPaciente\":" + Out::q(d.idPaciente)
               + ",\"de\":" + Out::q(d.de) + ",\"a\":" + Out::q(d.a)
               + ",\"motivo\":" + Out::q(d.motivo) + "}";
        }
        s += "]}";
        return s;
    }

private:
    // Red de ejemplo (tiempos de traslado en minutos)
    void cargarEjemplo() {
        agregarNodo({"H-CENTRAL", "Hospital Central",         "hospital"});
        agregarNodo({"H-NORTE",   "Hospital Norte",           "hospital"});
        agregarNodo({"H-SUR",     "Hospital Regional Sur",    "hospital"});
        agregarNodo({"C-DIAL",    "Centro de Dialisis",       "centro"});
        agregarNodo({"C-REHAB",   "Centro de Rehabilitacion", "centro"});
        agregarNodo({"E-CARDIO",  "Cardiologia",              "especialista"});
        agregarNodo({"E-NEURO",   "Neurologia",               "especialista"});
        agregarNodo({"E-TRAUMA",  "Traumatologia",            "especialista"});

        agregarArista("H-NORTE",  "H-CENTRAL", 25, "Cuidados intensivos");
        agregarArista("H-SUR",    "H-CENTRAL", 40, "Cuidados intensivos");
        agregarArista("H-CENTRAL","E-CARDIO",   5, "Evaluacion cardiaca");
        agregarArista("H-CENTRAL","E-NEURO",    8, "Evaluacion neurologica");
        agregarArista("H-CENTRAL","E-TRAUMA",  10, "Politraumatismo");
        agregarArista("H-CENTRAL","C-DIAL",    20, "Falla renal");
        agregarArista("E-TRAUMA", "C-REHAB",   30, "Rehabilitacion");
        agregarArista("E-NEURO",  "C-REHAB",   35, "Rehabilitacion neurologica");
        agregarArista("H-NORTE",  "E-CARDIO",  30, "Derivacion directa");
        agregarArista("H-SUR",    "E-TRAUMA",  35, "Derivacion directa");
        agregarArista("C-DIAL",   "H-SUR",     45, "Seguimiento");
    }
};
