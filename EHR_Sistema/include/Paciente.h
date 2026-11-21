#pragma once
#include <string>
#include <vector>
#include "SOFA.h"

// ============================================================
//  Paciente.h  –  Modelo de dominio principal
//  Representa el perfil completo de un paciente hospitalario.
// ============================================================

struct HistoriaClinica {
    std::string idHistoria;
    std::string fechaRegistro;   // "YYYY-MM-DD"
    std::string diagnostico;
    std::string medicamentos;    // lista separada por comas
    std::string archivoDICOM;    // ruta o referencia de imagen
    std::string mongoId;         // _id asignado por MongoDB

    HistoriaClinica() = default;
    HistoriaClinica(const std::string& id,
                    const std::string& fecha,
                    const std::string& diag,
                    const std::string& meds  = "",
                    const std::string& dicom = "")
        : idHistoria(id), fechaRegistro(fecha),
          diagnostico(diag), medicamentos(meds), archivoDICOM(dicom) {}
};

struct Paciente {
    std::string idPaciente;      // DNI o código QR único
    std::string nombre;
    std::string tipoSangre;
    std::vector<std::string> alergias;

    int         edad = 0;        // 0 = no registrada
    std::string diagnostico;     // diagnostico principal (opcional)

    // Métricas de gravedad para UCI / Triage
    int    nivelTriage;          // 1 (crítico) – 5 (no urgente)
    double scoreSOFA;            // 0–24, mayor = más grave
    bool   enUCI;

    // Parametros clinicos para calcular el SOFA automaticamente.
    // Si sofaParams.medido == true, scoreSOFA = SOFA::total(sofaParams).
    ParametrosSOFA sofaParams;

    // Recalcula scoreSOFA si hay parametros clinicos medidos
    void recalcularSOFA() {
        if (sofaParams.medido) scoreSOFA = SOFA::total(sofaParams);
    }

    std::string mongoId;         // _id asignado por MongoDB

    std::vector<HistoriaClinica> historias;

    // ---- Constructores ----------------------------------------
    Paciente() : nivelTriage(5), scoreSOFA(0.0), enUCI(false) {}

    Paciente(const std::string& id,
             const std::string& nom,
             const std::string& sangre,
             int triage       = 5,
             double sofa      = 0.0,
             bool uci         = false)
        : idPaciente(id), nombre(nom), tipoSangre(sangre),
          nivelTriage(triage), scoreSOFA(sofa), enUCI(uci) {}

    // ---- Utilidades -------------------------------------------
    void agregarAlergia(const std::string& alergia) {
        alergias.push_back(alergia);
    }

    void agregarHistoria(const HistoriaClinica& h) {
        historias.push_back(h);
    }

    // Para debug / visualización
    std::string toString() const {
        std::string s  = "┌─ Paciente ──────────────────────────\n";
        s += "│ ID       : " + idPaciente + "\n";
        s += "│ Nombre   : " + nombre + "\n";
        s += "│ Sangre   : " + tipoSangre + "\n";
        s += "│ Triage   : " + std::to_string(nivelTriage) + "\n";
        s += "│ SOFA     : " + std::to_string(scoreSOFA) + "\n";
        s += "│ UCI      : " + std::string(enUCI ? "Sí" : "No") + "\n";
        if (!alergias.empty()) {
            s += "│ Alergias : ";
            for (const auto& a : alergias) s += a + " ";
            s += "\n";
        }
        s += "│ Historias: " + std::to_string(historias.size()) + " registro(s)\n";
        s += "└─────────────────────────────────────\n";
        return s;
    }
};
