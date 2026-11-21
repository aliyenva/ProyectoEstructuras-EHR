#pragma once
// ================================================================
//  HttpServer.h  –  Servidor HTTP/1.1 minimo con WinSock2
//  Sin dependencias externas (solo ws2_32, nativo de Windows).
//
//  - Escucha SOLO en 127.0.0.1 (no expuesto a la red).
//  - Un hilo por conexion; el handler de la API se serializa con un
//    mutex, porque las estructuras de datos no son thread-safe.
//  - Sirve archivos estaticos de 'webDir' (rutas que no empiezan por /api).
//  - Connection: close en todas las respuestas.
// ================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <map>
#include <functional>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <iostream>

struct HttpRequest {
    std::string metodo;
    std::string ruta;                                // sin query, decodificada
    std::map<std::string, std::string> query;        // decodificada
    std::string cuerpo;
};

struct HttpResponse {
    int         estado = 200;
    std::string tipo   = "application/json; charset=utf-8";
    std::string cuerpo;
};

class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

private:
    int         puerto;
    std::string webDir;
    Handler     handler;
    SOCKET      escucha = INVALID_SOCKET;
    std::mutex  mtx;

    static const size_t MAX_CUERPO = 1 << 20;   // 1 MB

    // ── Utilidades ───────────────────────────────────────────────
    static std::string urlDecode(const std::string& s) {
        std::string o;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '%' && i + 2 < s.size() + 0 && isxdigit((unsigned char)s[i+1])
                && isxdigit((unsigned char)s[i+2])) {
                o += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16);
                i += 2;
            } else if (s[i] == '+') o += ' ';
            else o += s[i];
        }
        return o;
    }

    static std::string textoEstado(int e) {
        switch (e) {
            case 200: return "OK";
            case 201: return "Created";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 409: return "Conflict";
            case 413: return "Payload Too Large";
            case 500: return "Internal Server Error";
            default:  return "OK";
        }
    }

    static std::string mime(const std::string& ruta) {
        auto p = ruta.find_last_of('.');
        std::string ext = (p == std::string::npos) ? "" : ruta.substr(p);
        if (ext == ".html") return "text/html; charset=utf-8";
        if (ext == ".css")  return "text/css; charset=utf-8";
        if (ext == ".js" || ext == ".mjs") return "text/javascript; charset=utf-8";
        if (ext == ".json") return "application/json; charset=utf-8";
        if (ext == ".svg")  return "image/svg+xml";
        if (ext == ".png")  return "image/png";
        if (ext == ".ico")  return "image/x-icon";
        return "application/octet-stream";
    }

    static void enviar(SOCKET c, const HttpResponse& r) {
        std::ostringstream h;
        h << "HTTP/1.1 " << r.estado << " " << textoEstado(r.estado) << "\r\n"
          << "Content-Type: " << r.tipo << "\r\n"
          << "Content-Length: " << r.cuerpo.size() << "\r\n"
          << "Cache-Control: no-store\r\n"
          << "Connection: close\r\n\r\n";
        std::string cab = h.str();
        send(c, cab.c_str(), (int)cab.size(), 0);
        size_t off = 0;
        while (off < r.cuerpo.size()) {
            int n = send(c, r.cuerpo.c_str() + off, (int)(r.cuerpo.size() - off), 0);
            if (n <= 0) break;
            off += n;
        }
    }

    // ── Archivos estaticos (con proteccion contra ../) ───────────
    HttpResponse estatico(std::string ruta) {
        HttpResponse r;
        if (ruta == "/") ruta = "/index.html";
        if (ruta.find("..") != std::string::npos || ruta.find(':') != std::string::npos ||
            ruta.find('\\') != std::string::npos) {
            r.estado = 400; r.tipo = "text/plain"; r.cuerpo = "Ruta invalida"; return r;
        }
        std::ifstream f(webDir + ruta, std::ios::binary);
        if (!f) { r.estado = 404; r.tipo = "text/plain; charset=utf-8"; r.cuerpo = "No encontrado"; return r; }
        std::stringstream ss; ss << f.rdbuf();
        r.cuerpo = ss.str();
        r.tipo = mime(ruta);
        return r;
    }

    // ── Atiende una conexion ─────────────────────────────────────
    void atender(SOCKET c) {
        DWORD to = 5000;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof to);

        std::string datos;
        char buf[4096];
        size_t finCab = std::string::npos;
        while (finCab == std::string::npos) {
            int n = recv(c, buf, sizeof buf, 0);
            if (n <= 0) { closesocket(c); return; }
            datos.append(buf, n);
            finCab = datos.find("\r\n\r\n");
            if (datos.size() > 65536 && finCab == std::string::npos) { closesocket(c); return; }
        }

        // Linea de peticion
        std::string cabeceras = datos.substr(0, finCab);
        std::istringstream ls(cabeceras);
        std::string linea, metodo, objetivo, version;
        std::getline(ls, linea);
        std::istringstream rl(linea);
        rl >> metodo >> objetivo >> version;

        // Content-Length
        size_t largo = 0;
        {
            std::string low = cabeceras;
            for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
            auto p = low.find("content-length:");
            if (p != std::string::npos) {
                try { largo = (size_t)std::stoul(low.substr(p + 15)); } catch (...) { largo = 0; }
            }
        }
        if (largo > MAX_CUERPO) {
            HttpResponse r; r.estado = 413; r.cuerpo = "{\"ok\":false,\"error\":\"Cuerpo demasiado grande\"}";
            enviar(c, r); closesocket(c); return;
        }
        std::string cuerpo = datos.substr(finCab + 4);
        while (cuerpo.size() < largo) {
            int n = recv(c, buf, sizeof buf, 0);
            if (n <= 0) break;
            cuerpo.append(buf, n);
        }
        if (cuerpo.size() > largo) cuerpo.resize(largo);
        // Ignorar BOM UTF-8 si el cliente lo envia
        if (cuerpo.size() >= 3 && (unsigned char)cuerpo[0] == 0xEF &&
            (unsigned char)cuerpo[1] == 0xBB && (unsigned char)cuerpo[2] == 0xBF)
            cuerpo.erase(0, 3);

        // Ruta y query
        HttpRequest req;
        req.metodo = metodo;
        req.cuerpo = cuerpo;
        std::string ruta = objetivo, qs;
        auto q = objetivo.find('?');
        if (q != std::string::npos) { ruta = objetivo.substr(0, q); qs = objetivo.substr(q + 1); }
        req.ruta = urlDecode(ruta);
        std::istringstream qss(qs);
        std::string par;
        while (std::getline(qss, par, '&')) {
            auto e = par.find('=');
            if (e == std::string::npos) req.query[urlDecode(par)] = "";
            else req.query[urlDecode(par.substr(0, e))] = urlDecode(par.substr(e + 1));
        }

        HttpResponse resp;
        try {
            if (req.ruta.rfind("/api/", 0) == 0 || req.ruta == "/api") {
                std::lock_guard<std::mutex> lock(mtx);   // una peticion a la vez
                resp = handler(req);
            } else if (metodo == "GET" || metodo == "HEAD") {
                resp = estatico(req.ruta);
            } else {
                resp.estado = 405; resp.tipo = "text/plain"; resp.cuerpo = "Metodo no permitido";
            }
        } catch (const std::exception& e) {
            resp.estado = 500;
            resp.tipo = "application/json; charset=utf-8";
            resp.cuerpo = std::string("{\"ok\":false,\"error\":\"") + e.what() + "\"}";
        }
        enviar(c, resp);
        shutdown(c, SD_SEND);
        closesocket(c);
    }

public:
    HttpServer(int puerto_, const std::string& webDir_, Handler h)
        : puerto(puerto_), webDir(webDir_), handler(std::move(h)) {}

    // Crea el socket y empieza a escuchar. false si falla (puerto ocupado, etc.)
    bool iniciar() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
        escucha = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (escucha == INVALID_SOCKET) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons((u_short)puerto);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // 127.0.0.1

        if (bind(escucha, (sockaddr*)&addr, sizeof addr) == SOCKET_ERROR) return false;
        if (listen(escucha, SOMAXCONN) == SOCKET_ERROR) return false;
        return true;
    }

    // Bucle principal (bloqueante)
    void correr() {
        while (true) {
            SOCKET c = accept(escucha, nullptr, nullptr);
            if (c == INVALID_SOCKET) continue;
            std::thread([this, c]() { atender(c); }).detach();
        }
    }
};
