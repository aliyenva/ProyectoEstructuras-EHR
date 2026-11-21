#pragma once
#include <string>
#include <sstream>
#include <cstdio>
#include "Paciente.h"

// ================================================================
//  Serializador.h  –  Salida JSON (UTF-8) para la API web
//  Solo escritura; el parseo de entrada usa namespace JSON (MongoAPI.h)
// ================================================================
namespace Out {

// Escapa una cadena para JSON (incluye caracteres de control)
inline std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o;
}

// "texto" ya escapado y entre comillas
inline std::string q(const std::string& s) { return "\"" + esc(s) + "\""; }

// Numero decimal con punto (independiente de la configuracion regional)
inline std::string num(double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}

inline std::string boolean(bool b) { return b ? "true" : "false"; }

// Paciente completo, con parametros y desglose SOFA
inline std::string pacienteJson(const Paciente& p) {
    ComponentesSOFA c = SOFA::calcular(p.sofaParams);
    std::ostringstream o;
    o << "{\"idPaciente\":" << q(p.idPaciente)
      << ",\"nombre\":"     << q(p.nombre)
      << ",\"edad\":"       << p.edad
      << ",\"tipoSangre\":" << q(p.tipoSangre)
      << ",\"diagnostico\":" << q(p.diagnostico)
      << ",\"alergias\":[";
    for (size_t i = 0; i < p.alergias.size(); i++) {
        if (i) o << ",";
        o << q(p.alergias[i]);
    }
    o << "],\"nivelTriage\":" << p.nivelTriage
      << ",\"scoreSOFA\":"    << num(p.scoreSOFA)
      << ",\"enUCI\":"        << boolean(p.enUCI)
      << ",\"mongoId\":"      << q(p.mongoId)
      << ",\"sofaMedido\":"   << boolean(p.sofaParams.medido)
      << ",\"sofaParams\":{"
      << "\"pao2fio2\":"      << num(p.sofaParams.pao2fio2)
      << ",\"ventilado\":"    << boolean(p.sofaParams.ventilado)
      << ",\"plaquetas\":"    << num(p.sofaParams.plaquetas)
      << ",\"bilirrubina\":"  << num(p.sofaParams.bilirrubina)
      << ",\"pam\":"          << num(p.sofaParams.pam)
      << ",\"vasopresor\":"   << p.sofaParams.vasopresor
      << ",\"glasgow\":"      << p.sofaParams.glasgow
      << ",\"creatinina\":"   << num(p.sofaParams.creatinina)
      << "},\"sofaComponentes\":{"
      << "\"respiratorio\":"   << c.respiratorio
      << ",\"coagulacion\":"   << c.coagulacion
      << ",\"hepatico\":"      << c.hepatico
      << ",\"cardiovascular\":" << c.cardiovascular
      << ",\"neurologico\":"   << c.neurologico
      << ",\"renal\":"         << c.renal
      << "}}";
    return o.str();
}

// Version resumida para listas, nodos y heap
inline std::string pacienteMini(const Paciente& p) {
    std::ostringstream o;
    o << "{\"id\":" << q(p.idPaciente)
      << ",\"nombre\":" << q(p.nombre)
      << ",\"nivelTriage\":" << p.nivelTriage
      << ",\"sofa\":" << num(p.scoreSOFA)
      << ",\"enUCI\":" << boolean(p.enUCI) << "}";
    return o.str();
}

} // namespace Out
