#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "Paciente.h"
#include "Serializador.h"

// ================================================================
//  ArbolRojoNegro.h
//  Propósito: Ranking en tiempo real de gravedad de pacientes UCI.
//  Clave de ordenamiento: scoreSOFA (double) + idPaciente como
//  desempate.  Mayor scoreSOFA = paciente más grave = raíz del
//  recorrido in-order descendente.
//
//  Complejidad:
//    - Insertar  : O(log n)
//    - Buscar    : O(log n)
//    - Eliminar  : O(log n)
//    - Recorrido : O(n)
// ================================================================

enum Color { ROJO, NEGRO };

struct NodoRN {
    Paciente dato;
    Color    color;
    NodoRN*  izq;
    NodoRN*  der;
    NodoRN*  padre;

    explicit NodoRN(const Paciente& p)
        : dato(p), color(ROJO), izq(nullptr), der(nullptr), padre(nullptr) {}
};

class ArbolRojoNegro {
private:
    NodoRN* raiz;
    NodoRN* NIL;   // nodo centinela (hoja negra)
    long    rotaciones_ = 0;   // contador acumulado (para el visualizador)

    // ── Comparación por clave: scoreSOFA desc, luego id asc ─────
    // Devuelve <0 si a < b, 0 si iguales, >0 si a > b
    static int comparar(const Paciente& a, const Paciente& b) {
        if (a.scoreSOFA != b.scoreSOFA)
            return (a.scoreSOFA < b.scoreSOFA) ? -1 : 1;
        return a.idPaciente.compare(b.idPaciente);
    }

    // ── Rotaciones ───────────────────────────────────────────────
    void rotarIzquierda(NodoRN* x) {
        rotaciones_++;
        NodoRN* y = x->der;
        x->der = y->izq;
        if (y->izq != NIL) y->izq->padre = x;
        y->padre = x->padre;
        if (x->padre == NIL)       raiz = y;
        else if (x == x->padre->izq) x->padre->izq = y;
        else                         x->padre->der = y;
        y->izq   = x;
        x->padre = y;
    }

    void rotarDerecha(NodoRN* x) {
        rotaciones_++;
        NodoRN* y = x->izq;
        x->izq = y->der;
        if (y->der != NIL) y->der->padre = x;
        y->padre = x->padre;
        if (x->padre == NIL)       raiz = y;
        else if (x == x->padre->der) x->padre->der = y;
        else                         x->padre->izq = y;
        y->der   = x;
        x->padre = y;
    }

    // ── Reparación tras inserción ─────────────────────────────────
    void repararInsercion(NodoRN* z) {
        while (z->padre->color == ROJO) {
            if (z->padre == z->padre->padre->izq) {
                NodoRN* tio = z->padre->padre->der;
                if (tio->color == ROJO) {              // Caso 1
                    z->padre->color         = NEGRO;
                    tio->color              = NEGRO;
                    z->padre->padre->color  = ROJO;
                    z = z->padre->padre;
                } else {
                    if (z == z->padre->der) {           // Caso 2
                        z = z->padre;
                        rotarIzquierda(z);
                    }
                    z->padre->color        = NEGRO;     // Caso 3
                    z->padre->padre->color = ROJO;
                    rotarDerecha(z->padre->padre);
                }
            } else {
                NodoRN* tio = z->padre->padre->izq;
                if (tio->color == ROJO) {
                    z->padre->color         = NEGRO;
                    tio->color              = NEGRO;
                    z->padre->padre->color  = ROJO;
                    z = z->padre->padre;
                } else {
                    if (z == z->padre->izq) {
                        z = z->padre;
                        rotarDerecha(z);
                    }
                    z->padre->color        = NEGRO;
                    z->padre->padre->color = ROJO;
                    rotarIzquierda(z->padre->padre);
                }
            }
        }
        raiz->color = NEGRO;
    }

    // ── Transplante (usado en eliminación) ───────────────────────
    void transplantar(NodoRN* u, NodoRN* v) {
        if (u->padre == NIL)        raiz = v;
        else if (u == u->padre->izq) u->padre->izq = v;
        else                         u->padre->der = v;
        v->padre = u->padre;
    }

    NodoRN* minimoSubarbol(NodoRN* x) {
        while (x->izq != NIL) x = x->izq;
        return x;
    }

    // ── Reparación tras eliminación ──────────────────────────────
    void repararEliminacion(NodoRN* x) {
        while (x != raiz && x->color == NEGRO) {
            if (x == x->padre->izq) {
                NodoRN* w = x->padre->der;
                if (w->color == ROJO) {
                    w->color         = NEGRO;
                    x->padre->color  = ROJO;
                    rotarIzquierda(x->padre);
                    w = x->padre->der;
                }
                if (w->izq->color == NEGRO && w->der->color == NEGRO) {
                    w->color = ROJO;
                    x = x->padre;
                } else {
                    if (w->der->color == NEGRO) {
                        w->izq->color = NEGRO;
                        w->color      = ROJO;
                        rotarDerecha(w);
                        w = x->padre->der;
                    }
                    w->color         = x->padre->color;
                    x->padre->color  = NEGRO;
                    w->der->color    = NEGRO;
                    rotarIzquierda(x->padre);
                    x = raiz;
                }
            } else {
                NodoRN* w = x->padre->izq;
                if (w->color == ROJO) {
                    w->color         = NEGRO;
                    x->padre->color  = ROJO;
                    rotarDerecha(x->padre);
                    w = x->padre->izq;
                }
                if (w->der->color == NEGRO && w->izq->color == NEGRO) {
                    w->color = ROJO;
                    x = x->padre;
                } else {
                    if (w->izq->color == NEGRO) {
                        w->der->color = NEGRO;
                        w->color      = ROJO;
                        rotarIzquierda(w);
                        w = x->padre->izq;
                    }
                    w->color         = x->padre->color;
                    x->padre->color  = NEGRO;
                    w->izq->color    = NEGRO;
                    rotarDerecha(x->padre);
                    x = raiz;
                }
            }
        }
        x->color = NEGRO;
    }

    // ── Búsqueda interna ─────────────────────────────────────────
    // CORRECCION: el arbol esta ordenado por (scoreSOFA, id), por lo que
    // buscar solo por id NO puede navegar comparando ids. Dos variantes:
    //   - buscarNodoClave : O(log n), requiere conocer (scoreSOFA, id)
    //   - buscarNodo      : O(n), recorrido en preorden (solo se conoce el id)
    NodoRN* buscarNodoClave(double sofa, const std::string& id,
                            std::vector<std::string>* camino = nullptr,
                            int* comparaciones = nullptr) const {
        Paciente clave;
        clave.scoreSOFA  = sofa;
        clave.idPaciente = id;
        NodoRN* actual = raiz;
        while (actual != NIL) {
            if (camino) camino->push_back(actual->dato.idPaciente);
            if (comparaciones) (*comparaciones)++;
            int cmp = comparar(clave, actual->dato);
            if (cmp == 0) return actual;
            actual = (cmp < 0) ? actual->izq : actual->der;
        }
        return NIL;
    }

    NodoRN* buscarNodoDFS(NodoRN* n, const std::string& id,
                          std::vector<std::string>* camino,
                          int* comparaciones) const {
        if (n == NIL) return NIL;
        if (camino) camino->push_back(n->dato.idPaciente);
        if (comparaciones) (*comparaciones)++;
        if (n->dato.idPaciente == id) return n;
        NodoRN* r = buscarNodoDFS(n->izq, id, camino, comparaciones);
        if (r != NIL) return r;
        return buscarNodoDFS(n->der, id, camino, comparaciones);
    }

    NodoRN* buscarNodo(const std::string& idPaciente,
                       std::vector<std::string>* camino = nullptr,
                       int* comparaciones = nullptr) const {
        return buscarNodoDFS(raiz, idPaciente, camino, comparaciones);
    }

    // ── Recorridos y exportacion ─────────────────────────────────
    void preOrden(NodoRN* n, std::vector<const Paciente*>& r) const {
        if (n == NIL) return;
        r.push_back(&n->dato); preOrden(n->izq, r); preOrden(n->der, r);
    }
    void inOrden(NodoRN* n, std::vector<const Paciente*>& r) const {
        if (n == NIL) return;
        inOrden(n->izq, r); r.push_back(&n->dato); inOrden(n->der, r);
    }
    void postOrden(NodoRN* n, std::vector<const Paciente*>& r) const {
        if (n == NIL) return;
        postOrden(n->izq, r); postOrden(n->der, r); r.push_back(&n->dato);
    }
    static std::string idsJson(const std::vector<const Paciente*>& v) {
        std::string s = "[";
        for (size_t i = 0; i < v.size(); i++) {
            if (i) s += ",";
            s += Out::q(v[i]->idPaciente);
        }
        return s + "]";
    }

    // Devuelve el JSON del subarbol; h = altura (hoja = 1), bh = altura negra
    // (nodos negros en el camino hasta una hoja, sin contar NIL)
    std::string nodoJson(NodoRN* n, int& h, int& bh) const {
        if (n == NIL) { h = 0; bh = 0; return "null"; }
        int hi, bi, hd, bd;
        std::string si = nodoJson(n->izq, hi, bi);
        std::string sd = nodoJson(n->der, hd, bd);
        h  = 1 + std::max(hi, hd);
        bh = (n->color == NEGRO ? 1 : 0) + std::max(bi, bd);
        std::string s = "{\"id\":" + Out::q(n->dato.idPaciente)
            + ",\"nombre\":" + Out::q(n->dato.nombre)
            + ",\"sofa\":" + Out::num(n->dato.scoreSOFA)
            + ",\"nivelTriage\":" + std::to_string(n->dato.nivelTriage)
            + ",\"color\":\"" + (n->color == ROJO ? "R" : "N") + "\""
            + ",\"altura\":" + std::to_string(h)
            + ",\"alturaNegra\":" + std::to_string(bh)
            + ",\"balance\":" + std::to_string(hi - hd)
            + ",\"izq\":" + si + ",\"der\":" + sd + "}";
        return s;
    }

    // ── Recorrido in-order descendente (mayor SOFA primero) ──────
    void inOrderDesc(NodoRN* nodo, std::vector<Paciente>& resultado) const {
        if (nodo == NIL) return;
        inOrderDesc(nodo->der, resultado);
        resultado.push_back(nodo->dato);
        inOrderDesc(nodo->izq, resultado);
    }

    // ── Visualización ─────────────────────────────────────────────
    void imprimirArbol(NodoRN* nodo, const std::string& prefijo, bool esIzq) const {
        if (nodo == NIL) return;
        std::cout << prefijo
                  << (esIzq ? "├── " : "└── ")
                  << "[" << (nodo->color == ROJO ? "R" : "N") << "] "
                  << nodo->dato.nombre
                  << " (SOFA=" << nodo->dato.scoreSOFA
                  << ", T" << nodo->dato.nivelTriage << ")\n";
        imprimirArbol(nodo->izq,  prefijo + (esIzq ? "│   " : "    "), true);
        imprimirArbol(nodo->der,  prefijo + (esIzq ? "│   " : "    "), false);
    }

    // ── Liberar memoria ──────────────────────────────────────────
    void liberarArbol(NodoRN* nodo) {
        if (nodo == NIL) return;
        liberarArbol(nodo->izq);
        liberarArbol(nodo->der);
        delete nodo;
    }

public:
    // ── Constructor / Destructor ─────────────────────────────────
    ArbolRojoNegro() {
        NIL         = new NodoRN(Paciente());
        NIL->color  = NEGRO;
        NIL->izq    = NIL->der = NIL->padre = NIL;
        raiz        = NIL;
    }

    ~ArbolRojoNegro() {
        liberarArbol(raiz);
        delete NIL;
    }

    bool estaVacio() const { return raiz == NIL; }

    // ── Insertar ─────────────────────────────────────────────────
    void insertar(const Paciente& p) {
        NodoRN* z    = new NodoRN(p);
        z->izq = z->der = z->padre = NIL;

        NodoRN* y = NIL;
        NodoRN* x = raiz;

        while (x != NIL) {
            y = x;
            int cmp = comparar(z->dato, x->dato);
            if (cmp < 0) x = x->izq;
            else          x = x->der;
        }
        z->padre = y;

        if (y == NIL)          raiz    = z;
        else if (comparar(z->dato, y->dato) < 0) y->izq = z;
        else                   y->der  = z;

        repararInsercion(z);
    }

    // ── Buscar por ID (O(n): el arbol se ordena por SOFA, no por id) ──
    Paciente* buscar(const std::string& idPaciente) {
        NodoRN* n = buscarNodo(idPaciente);
        return (n == NIL) ? nullptr : &n->dato;
    }

    // ── Buscar por clave compuesta (SOFA, id): O(log n) ──────────
    // 'camino' recibe los ids visitados desde la raiz.
    Paciente* buscarPorClave(double sofa, const std::string& id,
                             std::vector<std::string>* camino = nullptr,
                             int* comparaciones = nullptr) {
        NodoRN* n = buscarNodoClave(sofa, id, camino, comparaciones);
        return (n == NIL) ? nullptr : &n->dato;
    }

    // ── Buscar por id con traza (recorrido preorden, O(n)) ───────
    Paciente* buscarPorIdConTraza(const std::string& id,
                                  std::vector<std::string>& camino,
                                  int& comparaciones) {
        NodoRN* n = buscarNodo(id, &camino, &comparaciones);
        return (n == NIL) ? nullptr : &n->dato;
    }

    // ── Actualizar score SOFA (remoción + reinserción) ──────────
    bool actualizarSOFA(const std::string& idPaciente, double nuevoSOFA) {
        NodoRN* n = buscarNodo(idPaciente);
        if (n == NIL) return false;
        Paciente p = n->dato;
        eliminar(p.scoreSOFA, idPaciente);
        p.scoreSOFA = nuevoSOFA;
        insertar(p);
        return true;
    }

    // ── Eliminar por clave (SOFA, id): O(log n) ──────────────────
    bool eliminar(double sofa, const std::string& idPaciente) {
        NodoRN* z = buscarNodoClave(sofa, idPaciente);
        if (z == NIL) return false;
        eliminarNodo(z);
        return true;
    }

    // ── Eliminar solo con id (O(n) para localizar el nodo) ───────
    bool eliminar(const std::string& idPaciente) {
        NodoRN* z = buscarNodo(idPaciente);
        if (z == NIL) return false;
        eliminarNodo(z);
        return true;
    }

private:
    // Eliminacion de un nodo ya localizado (CLRS)
    void eliminarNodo(NodoRN* z) {

        NodoRN* y = z;
        NodoRN* x;
        Color colorOriginal = y->color;

        if (z->izq == NIL) {
            x = z->der;
            transplantar(z, z->der);
        } else if (z->der == NIL) {
            x = z->izq;
            transplantar(z, z->izq);
        } else {
            y = minimoSubarbol(z->der);
            colorOriginal = y->color;
            x = y->der;
            if (y->padre == z) {
                x->padre = y;
            } else {
                transplantar(y, y->der);
                y->der         = z->der;
                y->der->padre  = y;
            }
            transplantar(z, y);
            y->izq         = z->izq;
            y->izq->padre  = y;
            y->color       = z->color;
        }
        delete z;
        if (colorOriginal == NEGRO) repararEliminacion(x);
    }

public:
    long rotaciones() const { return rotaciones_; }

    // ── Exportacion JSON para el visualizador ────────────────────
    //  Arbol anidado + recorridos + metricas (altura, altura negra)
    std::string exportarJson() const {
        int h = 0, bh = 0;
        std::string arbol = nodoJson(raiz, h, bh);
        std::vector<const Paciente*> pre, in, post;
        preOrden(raiz, pre); inOrden(raiz, in); postOrden(raiz, post);
        return "{\"tamanio\":" + std::to_string(in.size())
            + ",\"altura\":" + std::to_string(h)
            + ",\"alturaNegra\":" + std::to_string(bh)
            + ",\"rotaciones\":" + std::to_string(rotaciones_)
            + ",\"criterio\":\"(scoreSOFA, idPaciente) ascendente\""
            + ",\"raiz\":" + arbol
            + ",\"inorden\":" + idsJson(in)
            + ",\"preorden\":" + idsJson(pre)
            + ",\"postorden\":" + idsJson(post) + "}";
    }

    // ── Ranking descendente (más grave primero) ──────────────────
    std::vector<Paciente> rankingUCI() const {
        std::vector<Paciente> lista;
        inOrderDesc(raiz, lista);
        return lista;
    }

    // ── Visualización del árbol ──────────────────────────────────
    void mostrar() const {
        if (raiz == NIL) {
            std::cout << "  (Árbol vacío)\n";
            return;
        }
        std::cout << "  Árbol Rojo-Negro – UCI (SOFA desc):\n";
        imprimirArbol(raiz->izq, "  ", true);
        std::cout << "  └── [" << (raiz->color == ROJO ? "R" : "N") << "] "
                  << raiz->dato.nombre
                  << " (SOFA=" << raiz->dato.scoreSOFA << ") [RAIZ]\n";
        imprimirArbol(raiz->der, "  ", false);
    }

    int tamanio() const {
        std::vector<Paciente> v;
        inOrderDesc(raiz, v);
        return static_cast<int>(v.size());
    }
};
