#pragma once
#include <algorithm>

// ================================================================
//  SOFA.h  –  Calculo automatico del score SOFA
//  (Sequential Organ Failure Assessment, Vincent et al. 1996)
//
//  Seis sistemas, 0-4 puntos cada uno  ->  total 0-24.
//  Los valores por defecto de ParametrosSOFA son "normales" (0 pts),
//  de modo que un parametro no medido no suma puntos.
// ================================================================

struct ParametrosSOFA {
    bool   medido      = false;  // true = el score se calcula con estos datos
    double pao2fio2    = 400;    // Respiratorio: PaO2/FiO2 (mmHg)
    bool   ventilado   = false;  // soporte ventilatorio mecanico
    double plaquetas   = 200;    // Coagulacion: plaquetas (x10^3/uL)
    double bilirrubina = 0.5;    // Hepatico: bilirrubina (mg/dL)
    double pam         = 80;     // Cardiovascular: presion arterial media (mmHg)
    int    vasopresor  = 0;      // 0 ninguno | 1 dopa<=5 o dobutamina
                                 // 2 dopa>5, epi<=0.1 o norepi<=0.1
                                 // 3 dopa>15, epi>0.1 o norepi>0.1
    int    glasgow     = 15;     // Neurologico: Glasgow (3-15)
    double creatinina  = 0.8;    // Renal: creatinina (mg/dL)
};

struct ComponentesSOFA {
    int respiratorio    = 0;
    int coagulacion     = 0;
    int hepatico        = 0;
    int cardiovascular  = 0;
    int neurologico     = 0;
    int renal           = 0;
    int total() const {
        return respiratorio + coagulacion + hepatico +
               cardiovascular + neurologico + renal;
    }
};

namespace SOFA {

inline ComponentesSOFA calcular(const ParametrosSOFA& p) {
    ComponentesSOFA c;

    // Respiratorio (PaO2/FiO2)
    if      (p.pao2fio2 < 100 && p.ventilado) c.respiratorio = 4;
    else if (p.pao2fio2 < 200 && p.ventilado) c.respiratorio = 3;
    else if (p.pao2fio2 < 300)                c.respiratorio = 2;
    else if (p.pao2fio2 < 400)                c.respiratorio = 1;

    // Coagulacion (plaquetas)
    if      (p.plaquetas < 20)  c.coagulacion = 4;
    else if (p.plaquetas < 50)  c.coagulacion = 3;
    else if (p.plaquetas < 100) c.coagulacion = 2;
    else if (p.plaquetas < 150) c.coagulacion = 1;

    // Hepatico (bilirrubina)
    if      (p.bilirrubina >= 12.0) c.hepatico = 4;
    else if (p.bilirrubina >= 6.0)  c.hepatico = 3;
    else if (p.bilirrubina >= 2.0)  c.hepatico = 2;
    else if (p.bilirrubina >= 1.2)  c.hepatico = 1;

    // Cardiovascular (PAM / vasopresores)
    int vaso = std::max(0, std::min(3, p.vasopresor));
    if (vaso > 0)          c.cardiovascular = vaso + 1;   // 2, 3 o 4
    else if (p.pam < 70)   c.cardiovascular = 1;

    // Neurologico (Glasgow)
    int g = std::max(3, std::min(15, p.glasgow));
    if      (g < 6)   c.neurologico = 4;
    else if (g <= 9)  c.neurologico = 3;
    else if (g <= 12) c.neurologico = 2;
    else if (g <= 14) c.neurologico = 1;

    // Renal (creatinina)
    if      (p.creatinina >= 5.0) c.renal = 4;
    else if (p.creatinina >= 3.5) c.renal = 3;
    else if (p.creatinina >= 2.0) c.renal = 2;
    else if (p.creatinina >= 1.2) c.renal = 1;

    return c;
}

inline double total(const ParametrosSOFA& p) { return calcular(p).total(); }

} // namespace SOFA
