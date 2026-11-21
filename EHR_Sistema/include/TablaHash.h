#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <stdexcept>
#include "Paciente.h"
#include "Serializador.h"

// ================================================================
//  TablaHash.h  –  Hashing Dinámico (Linear Hashing)
//  Propósito: Acceso O(1) promedio al perfil del paciente por ID
//             (DNI / código QR).
//
//  Estrategia: Linear Hashing con encadenamiento por listas.
//    - La tabla crece bucket a bucket (no duplica de golpe).
//    - Factor de carga umbral configurable (default 0.75).
//    - Se mantiene un puntero de split `sp` y nivel `nivel`.
//
//  Complejidad:
//    - Insertar  : O(1) amortizado
//    - Buscar    : O(1) promedio
//    - Eliminar  : O(1) promedio
// ================================================================

class TablaHash {
private:
    static const int CAPACIDAD_INICIAL = 8;
    static constexpr double CARGA_MAX  = 0.75;

    // Cada bucket es una lista encadenada de punteros a Paciente
    std::vector<std::list<Paciente>> tabla;

    int    nivel;        // nivel actual de hashing (h_nivel y h_{nivel+1})
    int    sp;           // split pointer: próximo bucket a dividir
    int    numElementos;

    // Estadisticas de splits (solo lectura desde fuera)
    int    splitsTotales      = 0;
    int    ultimoSplitBucket  = -1;
    int    ultimoSplitNuevo   = -1;
    int    ultimoSplitNivel   = 0;
    int    ultimoSplitMovidos = 0;

    // ── Función hash primaria ─────────────────────────────────────
    // Convierte el idPaciente (string) a un entero sin signo
    static unsigned int hashString(const std::string& clave) {
        unsigned int h = 2166136261u;  // FNV-1a offset basis
        for (char c : clave) {
            h ^= static_cast<unsigned char>(c);
            h *= 16777619u;
        }
        return h;
    }

    // ── Hash de nivel n ───────────────────────────────────────────
    int hashNivel(const std::string& clave, int n) const {
        int m = 1 << n;    // 2^n
        return static_cast<int>(hashString(clave) % static_cast<unsigned int>(m));
    }

    // ── Determinar bucket correcto ────────────────────────────────
    int bucketPara(const std::string& clave) const {
        int h = hashNivel(clave, nivel);
        if (h < sp) h = hashNivel(clave, nivel + 1);
        return h;
    }

    double factorCarga() const {
        return static_cast<double>(numElementos) /
               static_cast<double>(tabla.size());
    }

    // ── Dividir el bucket apuntado por sp ─────────────────────────
    void dividir() {
        tabla.push_back(std::list<Paciente>());   // nuevo bucket al final

        std::list<Paciente> viejo = tabla[sp];
        tabla[sp].clear();
        int nuevoBucket = sp + (1 << nivel);  // bucket nuevo = sp + 2^nivel

        // Registro del split para el visualizador
        splitsTotales++;
        ultimoSplitBucket = sp;
        ultimoSplitNuevo  = nuevoBucket;
        ultimoSplitNivel  = nivel;
        ultimoSplitMovidos = 0;

        for (const Paciente& p : viejo) {
            int dest = hashNivel(p.idPaciente, nivel + 1);
            if (dest != sp) ultimoSplitMovidos++;
            tabla[dest].push_back(p);
        }

        sp++;
        int limite = 1 << nivel;  // 2^nivel
        if (sp >= limite) {
            nivel++;
            sp = 0;
        }
    }

public:
    TablaHash() : nivel(3), sp(0), numElementos(0) {
        // nivel=3 → capacidad inicial = 2^3 = 8 buckets
        tabla.resize(CAPACIDAD_INICIAL);
    }

    // ── Insertar ─────────────────────────────────────────────────
    void insertar(const Paciente& p) {
        // Si ya existe, actualizar
        int b = bucketPara(p.idPaciente);
        for (Paciente& existente : tabla[b]) {
            if (existente.idPaciente == p.idPaciente) {
                existente = p;
                return;
            }
        }
        tabla[b].push_back(p);
        numElementos++;

        if (factorCarga() > CARGA_MAX) dividir();
    }

    // ── Buscar por ID ────────────────────────────────────────────
    Paciente* buscar(const std::string& idPaciente) {
        int b = bucketPara(idPaciente);
        for (Paciente& p : tabla[b]) {
            if (p.idPaciente == idPaciente) return &p;
        }
        return nullptr;
    }

    const Paciente* buscar(const std::string& idPaciente) const {
        int b = bucketPara(idPaciente);
        for (const Paciente& p : tabla[b]) {
            if (p.idPaciente == idPaciente) return &p;
        }
        return nullptr;
    }

    // ── Eliminar ─────────────────────────────────────────────────
    bool eliminar(const std::string& idPaciente) {
        int b = bucketPara(idPaciente);
        auto& lista = tabla[b];
        for (auto it = lista.begin(); it != lista.end(); ++it) {
            if (it->idPaciente == idPaciente) {
                lista.erase(it);
                numElementos--;
                return true;
            }
        }
        return false;
    }

    // ── Actualizar un paciente ────────────────────────────────────
    bool actualizar(const Paciente& p) {
        int b = bucketPara(p.idPaciente);
        for (Paciente& existente : tabla[b]) {
            if (existente.idPaciente == p.idPaciente) {
                existente = p;
                return true;
            }
        }
        return false;
    }

    // ── Estadísticas ─────────────────────────────────────────────
    int  elementos()     const { return numElementos; }
    int  buckets()       const { return static_cast<int>(tabla.size()); }
    double carga()       const { return factorCarga(); }

    // ── Visualizar distribución de buckets ───────────────────────
    void mostrar() const {
        std::cout << "  Tabla Hash Dinámica (Linear Hashing)\n"
                  << "  Nivel=" << nivel << "  SP=" << sp
                  << "  Elementos=" << numElementos
                  << "  Buckets=" << tabla.size()
                  << "  Carga=" << factorCarga() << "\n";
        std::cout << "  ┌──────────────────────────────────────\n";
        for (int i = 0; i < static_cast<int>(tabla.size()); i++) {
            std::cout << "  │ [" << i << "] ";
            if (tabla[i].empty()) {
                std::cout << "(vacío)";
            } else {
                for (const Paciente& p : tabla[i]) {
                    std::cout << p.idPaciente << ":" << p.nombre << "  ";
                }
            }
            std::cout << "\n";
        }
        std::cout << "  └──────────────────────────────────────\n";
    }

    // ── Datos para el visualizador ───────────────────────────────
    int splits()  const { return splitsTotales; }
    int bucketDe(const std::string& id) const { return bucketPara(id); }

    // Busqueda con traza: bucket consultado y comparaciones dentro de la cadena
    Paciente* buscarConTraza(const std::string& id, int& bucket, int& comparaciones) {
        bucket = bucketPara(id);
        comparaciones = 0;
        for (Paciente& p : tabla[bucket]) {
            comparaciones++;
            if (p.idPaciente == id) return &p;
        }
        return nullptr;
    }

    std::string exportarJson() const {
        int colisiones = 0, maxCadena = 0;
        std::string b = "[";
        for (size_t i = 0; i < tabla.size(); i++) {
            int len = (int)tabla[i].size();
            if (len > 1) colisiones += len - 1;
            if (len > maxCadena) maxCadena = len;
            if (i) b += ",";
            b += "{\"indice\":" + std::to_string(i)
               + ",\"puntero\":" + (static_cast<int>(i) == sp ? "true" : "false")
               + ",\"items\":[";
            bool primero = true;
            for (const Paciente& p : tabla[i]) {
                if (!primero) b += ",";
                primero = false;
                b += "{\"id\":" + Out::q(p.idPaciente)
                   + ",\"nombre\":" + Out::q(p.nombre)
                   + ",\"hash\":" + std::to_string(hashString(p.idPaciente)) + "}";
            }
            b += "]}";
        }
        b += "]";
        std::string split = "null";
        if (splitsTotales > 0)
            split = "{\"bucket\":" + std::to_string(ultimoSplitBucket)
                  + ",\"nuevoBucket\":" + std::to_string(ultimoSplitNuevo)
                  + ",\"nivel\":" + std::to_string(ultimoSplitNivel)
                  + ",\"movidos\":" + std::to_string(ultimoSplitMovidos) + "}";
        return "{\"nivel\":" + std::to_string(nivel)
            + ",\"sp\":" + std::to_string(sp)
            + ",\"elementos\":" + std::to_string(numElementos)
            + ",\"buckets\":" + std::to_string(tabla.size())
            + ",\"carga\":" + Out::num(factorCarga())
            + ",\"cargaMax\":" + Out::num(CARGA_MAX)
            + ",\"colisiones\":" + std::to_string(colisiones)
            + ",\"cadenaMax\":" + std::to_string(maxCadena)
            + ",\"splitsTotales\":" + std::to_string(splitsTotales)
            + ",\"ultimoSplit\":" + split
            + ",\"tabla\":" + b + "}";
    }

    // ── Iteración completa (para carga masiva) ───────────────────
    std::vector<Paciente> todos() const {
        std::vector<Paciente> resultado;
        for (const auto& bucket : tabla) {
            for (const Paciente& p : bucket) {
                resultado.push_back(p);
            }
        }
        return resultado;
    }
};
