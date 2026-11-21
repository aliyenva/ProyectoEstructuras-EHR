#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include "Paciente.h"
#include "MongoAPI.h"
#include "TablaHash.h"
#include "ArbolRojoNegro.h"
#include "ColaPrioridad.h"

// ================================================================
//  CargadorDatos.h
//  Propósito: Poblar las tres estructuras en memoria al arrancar.
//
//  Estrategia de carga:
//    1. Intentar GET a MongoDB (nube).
//    2. Si falla (sin conexión), cargar desde data/pacientes.csv.
//
//  Formato CSV (data/pacientes.csv):
//    idPaciente,nombre,tipoSangre,nivelTriage,scoreSOFA,enUCI,alergias
//    P001,Juan Pérez,O+,2,12.5,true,"penicilina;aspirina"
// ================================================================

class CargadorDatos {
public:

    struct ResultadoCarga {
        int    cargados    = 0;
        bool   desdeNube   = false;
        std::string fuente;
    };

    // ── Cargar datos en las tres estructuras ─────────────────────
    static ResultadoCarga cargar(TablaHash&       hash,
                                  ArbolRojoNegro&  rbt,
                                  ColaPrioridad&   cola,
                                  MongoAPI&        api,
                                  const std::string& archivoCSV = "data/pacientes.csv") {
        ResultadoCarga res;

        // 1. Intentar desde la nube
        std::cout << "\n  [Cargador] Conectando con MongoDB...\n";
        std::vector<Paciente> lista;

        if (api.estaConectado()) {
            lista = api.consultarTodos();
            if (!lista.empty()) {
                res.desdeNube = true;
                res.fuente    = "MongoDB (nube)";
                std::cout << "  [Cargador] Conexión exitosa. "
                          << lista.size() << " paciente(s) encontrado(s) en la nube.\n";
            } else {
                std::cout << "  [Cargador] Nube conectada pero sin datos. "
                             "Cargando desde CSV y subiendo...\n";
                lista = leerCSV(archivoCSV);
                // Subir cada uno a MongoDB
                for (Paciente& p : lista) {
                    std::string id = api.insertar(p);
                    if (!id.empty()) p.mongoId = id;
                }
                res.fuente = "CSV → subido a MongoDB";
            }
        } else {
            std::cout << "  [Cargador] Sin conexión. Usando archivo local: "
                      << archivoCSV << "\n";
            lista  = leerCSV(archivoCSV);
            res.fuente = "CSV local (offline)";
        }

        // 2. Poblar estructuras en memoria (solo pacientes con ID válido)
        for (const Paciente& p : lista) {
            if (p.idPaciente.empty()) continue;
            hash.insertar(p);
            if (p.enUCI) rbt.insertar(p);
            cola.insertar(p);
            res.cargados++;
        }

        std::cout << "  [Cargador] " << res.cargados
                  << " paciente(s) cargado(s) desde: " << res.fuente << "\n\n";
        return res;
    }

    // ── Leer CSV local ───────────────────────────────────────────
    static std::vector<Paciente> leerCSV(const std::string& ruta) {
        std::vector<Paciente> lista;
        std::ifstream archivo(ruta);
        if (!archivo.is_open()) {
            std::cerr << "  [Cargador] No se pudo abrir: " << ruta << "\n";
            return lista;
        }

        std::string linea;
        std::getline(archivo, linea);  // saltar cabecera

        while (std::getline(archivo, linea)) {
            if (linea.empty()) continue;
            Paciente p = parsearLineaCSV(linea);
            if (!p.idPaciente.empty()) lista.push_back(p);
        }
        return lista;
    }

    // ── Guardar lista actual en CSV ──────────────────────────────
    static bool guardarCSV(const std::vector<Paciente>& lista,
                            const std::string& ruta = "data/pacientes.csv") {
        std::ofstream archivo(ruta);
        if (!archivo.is_open()) return false;

        archivo << "idPaciente,nombre,tipoSangre,nivelTriage,scoreSOFA,enUCI,alergias,"
                   "edad,diagnostico,sofaMedido,pao2fio2,ventilado,plaquetas,bilirrubina,"
                   "pam,vasopresor,glasgow,creatinina\n";
        for (const Paciente& p : lista) {
            std::string alerg;
            for (size_t i = 0; i < p.alergias.size(); i++) {
                if (i) alerg += ";";
                alerg += p.alergias[i];
            }
            archivo << p.idPaciente  << ","
                    << p.nombre      << ","
                    << p.tipoSangre  << ","
                    << p.nivelTriage << ","
                    << p.scoreSOFA   << ","
                    << (p.enUCI ? "true" : "false") << ","
                    << "\"" << alerg << "\","
                    << p.edad << ",\"" << p.diagnostico << "\","
                    << (p.sofaParams.medido ? "true" : "false") << ","
                    << p.sofaParams.pao2fio2 << ","
                    << (p.sofaParams.ventilado ? "true" : "false") << ","
                    << p.sofaParams.plaquetas << "," << p.sofaParams.bilirrubina << ","
                    << p.sofaParams.pam << "," << p.sofaParams.vasopresor << ","
                    << p.sofaParams.glasgow << "," << p.sofaParams.creatinina << "\n";
        }
        return true;
    }

private:
    // ── Parsear una línea CSV ─────────────────────────────────────
    static Paciente parsearLineaCSV(const std::string& linea) {
        Paciente p;
        std::vector<std::string> campos;
        bool enComillas = false;
        std::string campo;

        for (char c : linea) {
            if (c == '"') {
                enComillas = !enComillas;
            } else if (c == ',' && !enComillas) {
                campos.push_back(campo);
                campo.clear();
            } else {
                campo += c;
            }
        }
        campos.push_back(campo);

        if (campos.size() < 6) return p;

        p.idPaciente  = campos[0];
        p.nombre      = campos[1];
        p.tipoSangre  = campos[2];
        try { p.nivelTriage = std::stoi(campos[3]); } catch(...) { p.nivelTriage = 5; }
        try { p.scoreSOFA   = std::stod(campos[4]); } catch(...) { p.scoreSOFA   = 0; }
        p.enUCI       = (campos[5] == "true" || campos[5] == "1");

        if (campos.size() >= 7 && !campos[6].empty()) {
            std::istringstream ss(campos[6]);
            std::string alergia;
            while (std::getline(ss, alergia, ';')) {
                if (!alergia.empty()) p.alergias.push_back(alergia);
            }
        }

        // Columnas opcionales (CSV antiguos tienen solo 7):
        // 7 edad, 8 diagnostico, 9 sofaMedido, 10 pao2fio2, 11 ventilado,
        // 12 plaquetas, 13 bilirrubina, 14 pam, 15 vasopresor, 16 glasgow, 17 creatinina
        auto num = [&](size_t i, double def) {
            if (i >= campos.size() || campos[i].empty()) return def;
            try { return std::stod(campos[i]); } catch (...) { return def; }
        };
        if (campos.size() > 7) p.edad = (int)num(7, 0);
        if (campos.size() > 8) p.diagnostico = campos[8];
        if (campos.size() > 9) {
            p.sofaParams.medido      = (campos[9] == "true" || campos[9] == "1");
            p.sofaParams.pao2fio2    = num(10, 400);
            p.sofaParams.ventilado   = (campos.size() > 11 && (campos[11] == "true" || campos[11] == "1"));
            p.sofaParams.plaquetas   = num(12, 200);
            p.sofaParams.bilirrubina = num(13, 0.5);
            p.sofaParams.pam         = num(14, 80);
            p.sofaParams.vasopresor  = (int)num(15, 0);
            p.sofaParams.glasgow     = (int)num(16, 15);
            p.sofaParams.creatinina  = num(17, 0.8);
            p.recalcularSOFA();
        }
        return p;
    }
};
