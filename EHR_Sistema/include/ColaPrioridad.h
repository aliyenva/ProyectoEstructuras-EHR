#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include "Paciente.h"
#include "Serializador.h"

// ================================================================
//  ColaPrioridad.h  –  Max-Heap para Triage Hospitalario
//  Propósito: Despacho del paciente más grave en O(1).
//
//  Prioridad compuesta:
//    1. nivelTriage más bajo = más urgente (Nivel 1 > Nivel 5)
//    2. Desempate: mayor scoreSOFA
//    3. Desempate final: idPaciente lexicográfico
//
//  Complejidad:
//    - Insertar (push)   : O(log n)
//    - Extraer máximo    : O(log n)
//    - Ver máximo (peek) : O(1)
// ================================================================

struct EntradaTriage {
    Paciente paciente;
    int      prioridad;   // calculada al momento de insertar (menor = más urgente)

    EntradaTriage() : prioridad(0) {}
    explicit EntradaTriage(const Paciente& p)
        : paciente(p), prioridad(p.nivelTriage) {}
};

// Intercambio realizado dentro del heap (idDe estaba en 'de' y pasa a 'a')
struct Movimiento {
    int de = 0, a = 0;
    std::string idDe, idA;
};

class ColaPrioridad {
private:
    std::vector<EntradaTriage> heap;
    std::vector<Movimiento>    movimientos;      // de la ultima operacion
    long                       intercambiosTotales = 0;

    // ── Comparador: "más urgente" ─────────────────────────────────
    // Retorna true si a tiene mayor prioridad que b
    static bool masUrgente(const EntradaTriage& a, const EntradaTriage& b) {
        if (a.prioridad != b.prioridad)
            return a.prioridad < b.prioridad;          // nivelTriage más bajo
        if (a.paciente.scoreSOFA != b.paciente.scoreSOFA)
            return a.paciente.scoreSOFA > b.paciente.scoreSOFA; // SOFA más alto
        return a.paciente.idPaciente < b.paciente.idPaciente;
    }

    int padre(int i)  const { return (i - 1) / 2; }
    int izq(int i)    const { return 2 * i + 1; }
    int der(int i)    const { return 2 * i + 2; }

    // ── Sube el elemento en posición i hasta su lugar correcto ────
    void subirHeap(int i) {
        while (i > 0 && masUrgente(heap[i], heap[padre(i)])) {
            registrar(i, padre(i));
            std::swap(heap[i], heap[padre(i)]);
            i = padre(i);
        }
    }

    // ── Baja el elemento en posición i hasta su lugar correcto ───
    void bajarHeap(int i) {
        int n = static_cast<int>(heap.size());
        int mayor = i;

        if (izq(i) < n && masUrgente(heap[izq(i)], heap[mayor]))
            mayor = izq(i);
        if (der(i) < n && masUrgente(heap[der(i)], heap[mayor]))
            mayor = der(i);

        if (mayor != i) {
            registrar(i, mayor);
            std::swap(heap[i], heap[mayor]);
            bajarHeap(mayor);
        }
    }

    // Registra un intercambio (antes de hacerlo) para el visualizador
    void registrar(int a, int b) {
        Movimiento m;
        m.de = a; m.a = b;
        m.idDe = heap[a].paciente.idPaciente;
        m.idA  = heap[b].paciente.idPaciente;
        movimientos.push_back(m);
        intercambiosTotales++;
    }

public:
    ColaPrioridad() = default;

    bool    vacia()   const { return heap.empty(); }
    int     tamanio() const { return static_cast<int>(heap.size()); }

    // ── Insertar paciente ────────────────────────────────────────
    void insertar(const Paciente& p) {
        movimientos.clear();
        heap.emplace_back(p);
        subirHeap(static_cast<int>(heap.size()) - 1);
    }

    // ── Ver el más urgente sin extraer ──────────────────────────
    const Paciente& verUrgente() const {
        if (heap.empty()) throw std::runtime_error("Cola de Triage vacía.");
        return heap[0].paciente;
    }

    // ── Extraer el paciente más urgente ─────────────────────────
    Paciente extraerUrgente() {
        if (heap.empty()) throw std::runtime_error("Cola de Triage vacía.");
        movimientos.clear();
        Paciente resultado = heap[0].paciente;
        if (heap.size() > 1) {
            // El ultimo elemento pasa a la raiz y luego baja
            Movimiento m;
            m.de = (int)heap.size() - 1; m.a = 0;
            m.idDe = heap.back().paciente.idPaciente; m.idA = resultado.idPaciente;
            movimientos.push_back(m);
        }
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) bajarHeap(0);
        return resultado;
    }

    // ── Actualizar prioridad de un paciente (por ID) ─────────────
    // Reemplaza todos los datos del paciente (nombre, etc.) y reordena
    bool actualizarPaciente(const Paciente& nuevo) {
        movimientos.clear();
        for (int i = 0; i < static_cast<int>(heap.size()); i++) {
            if (heap[i].paciente.idPaciente == nuevo.idPaciente) {
                heap[i].paciente  = nuevo;
                heap[i].prioridad = nuevo.nivelTriage;
                subirHeap(i);
                // tras subir, el elemento puede estar en otra posicion
                int j = 0;
                for (; j < static_cast<int>(heap.size()); j++)
                    if (heap[j].paciente.idPaciente == nuevo.idPaciente) break;
                bajarHeap(j);
                return true;
            }
        }
        return false;
    }

    bool actualizarPrioridad(const std::string& idPaciente, int nuevoNivel, double nuevoSOFA) {
        movimientos.clear();
        for (int i = 0; i < static_cast<int>(heap.size()); i++) {
            if (heap[i].paciente.idPaciente == idPaciente) {
                heap[i].paciente.nivelTriage = nuevoNivel;
                heap[i].paciente.scoreSOFA   = nuevoSOFA;
                heap[i].prioridad            = nuevoNivel;
                // Re-heapify desde esta posición
                subirHeap(i);
                bajarHeap(i);
                return true;
            }
        }
        return false;
    }

    // ── Eliminar paciente de la cola (por ID) ─────────────────────
    bool eliminar(const std::string& idPaciente) {
        movimientos.clear();
        int pos = -1;
        for (int i = 0; i < static_cast<int>(heap.size()); i++) {
            if (heap[i].paciente.idPaciente == idPaciente) { pos = i; break; }
        }
        if (pos == -1) return false;

        heap[pos] = heap.back();
        heap.pop_back();
        if (pos < static_cast<int>(heap.size())) {
            subirHeap(pos);
            bajarHeap(pos);
        }
        return true;
    }

    // ── Visualización de la cola ─────────────────────────────────
    void mostrar() const {
        if (heap.empty()) {
            std::cout << "  (Cola de Triage vacía)\n";
            return;
        }
        std::cout << "  Cola de Prioridad – Triage (Max-Heap)\n";
        std::cout << "  ┌──────┬──────────────┬────────────────────┬────────┬───────┐\n";
        std::cout << "  │ Pos  │ ID           │ Nombre             │ Triage │ SOFA  │\n";
        std::cout << "  ├──────┼──────────────┼────────────────────┼────────┼───────┤\n";
        for (int i = 0; i < static_cast<int>(heap.size()); i++) {
            const Paciente& p = heap[i].paciente;
            printf("  │ %-4d │ %-12s │ %-18s │   %-4d │ %-5.1f │\n",
                   i, p.idPaciente.c_str(), p.nombre.c_str(),
                   p.nivelTriage, p.scoreSOFA);
        }
        std::cout << "  └──────┴──────────────┴────────────────────┴────────┴───────┘\n";
        std::cout << "  ► Siguiente en atender: " << heap[0].paciente.nombre
                  << " (Triage " << heap[0].paciente.nivelTriage << ")\n";
    }

    // ── Datos para el visualizador ───────────────────────────────
    long intercambios() const { return intercambiosTotales; }

    std::string movimientosJson() const {
        std::string s = "[";
        for (size_t i = 0; i < movimientos.size(); i++) {
            if (i) s += ",";
            s += "{\"de\":" + std::to_string(movimientos[i].de)
               + ",\"a\":" + std::to_string(movimientos[i].a)
               + ",\"idDe\":" + Out::q(movimientos[i].idDe)
               + ",\"idA\":" + Out::q(movimientos[i].idA) + "}";
        }
        return s + "]";
    }

    std::string exportarJson() const {
        std::string items = "[";
        for (size_t i = 0; i < heap.size(); i++) {
            const Paciente& p = heap[i].paciente;
            if (i) items += ",";
            items += "{\"pos\":" + std::to_string(i)
                   + ",\"padre\":" + std::to_string(i == 0 ? -1 : (int)((i - 1) / 2))
                   + ",\"id\":" + Out::q(p.idPaciente)
                   + ",\"nombre\":" + Out::q(p.nombre)
                   + ",\"nivelTriage\":" + std::to_string(p.nivelTriage)
                   + ",\"sofa\":" + Out::num(p.scoreSOFA)
                   + ",\"enUCI\":" + Out::boolean(p.enUCI) + "}";
        }
        items += "]";
        return "{\"tamanio\":" + std::to_string(heap.size())
            + ",\"criterio\":\"menor nivelTriage; desempate mayor SOFA; luego id\""
            + ",\"intercambiosTotales\":" + std::to_string(intercambiosTotales)
            + ",\"movimientos\":" + movimientosJson()
            + ",\"items\":" + items + "}";
    }

    // ── Lista completa como vector (copia) ───────────────────────
    std::vector<Paciente> listaTriage() const {
        std::vector<Paciente> lista;
        for (const auto& e : heap) lista.push_back(e.paciente);
        return lista;
    }
};
