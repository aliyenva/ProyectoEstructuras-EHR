#pragma once
#include <deque>
#include <string>
#include <ctime>
#include "Serializador.h"

// ================================================================
//  EventLog.h  –  Bitacora circular de eventos para la interfaz web
//  tipo: "api" (peticion HTTP al backend), "mongo" (llamada a la nube),
//        "sistema" (arranque, cambios de estado)
//  El frontend consulta /api/eventos?desde=N de forma incremental.
// ================================================================

struct Evento {
    long long   id = 0;
    std::string hora;        // HH:MM:SS
    std::string tipo;
    std::string metodo;      // GET, POST, ...
    std::string ruta;
    std::string estructura;  // hash, arbol, heap, grafo, mongo, ...
    std::string mensaje;
    int         codigo = 0;  // HTTP
    long        ms     = 0;
    bool        ok     = true;
};

class EventLog {
private:
    static const size_t MAXIMO = 300;
    std::deque<Evento> eventos;
    long long siguiente = 1;

    static std::string ahora() {
        std::time_t t = std::time(nullptr);
        char b[16];
        std::strftime(b, sizeof b, "%H:%M:%S", std::localtime(&t));
        return b;
    }

public:
    long long agregar(Evento e) {
        e.id   = siguiente++;
        e.hora = ahora();
        eventos.push_back(e);
        if (eventos.size() > MAXIMO) eventos.pop_front();
        return e.id;
    }

    long long ultimoId() const { return siguiente - 1; }

    std::string exportarJson(long long desde) const {
        std::string s = "{\"ultimo\":" + std::to_string(ultimoId()) + ",\"eventos\":[";
        bool primero = true;
        for (const auto& e : eventos) {
            if (e.id <= desde) continue;
            if (!primero) s += ",";
            primero = false;
            s += "{\"id\":" + std::to_string(e.id)
               + ",\"hora\":" + Out::q(e.hora)
               + ",\"tipo\":" + Out::q(e.tipo)
               + ",\"metodo\":" + Out::q(e.metodo)
               + ",\"ruta\":" + Out::q(e.ruta)
               + ",\"estructura\":" + Out::q(e.estructura)
               + ",\"mensaje\":" + Out::q(e.mensaje)
               + ",\"codigo\":" + std::to_string(e.codigo)
               + ",\"ms\":" + std::to_string(e.ms)
               + ",\"ok\":" + Out::boolean(e.ok) + "}";
        }
        return s + "]}";
    }
};
