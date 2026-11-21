#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include "Paciente.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

// ================================================================
//  MongoAPI.h  –  Cliente REST para la API MongoDB en la nube
//  URL: https://paginas-web-cr.com/Api/apis/mongodb.php
//  Coleccion: "pacientes_ehr"
//  HTTP via WinHTTP (nativo Windows, sin dependencias externas)
// ================================================================

// ================================================================
//  SECCION 1: Utilidades JSON
// ================================================================
namespace JSON {

// Elimina whitespace fuera de strings → simplifica todo el parsing
inline std::string aplanar(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    bool inStr = false;
    for (size_t i = 0; i < src.size(); i++) {
        char c = src[i];
        if (c == '"' && (i == 0 || src[i-1] != '\\')) inStr = !inStr;
        if (inStr || (c != ' ' && c != '\n' && c != '\r' && c != '\t'))
            out += c;
    }
    return out;
}

// Escapa caracteres especiales para JSON
inline std::string escapar(const std::string& s) {
    std::string out;
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

// Extrae valor string de "clave":"valor" en JSON ya aplanado
inline std::string getString(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\":\"";
    auto p = json.find(pat);
    if (p == std::string::npos) return "";
    p += pat.size();
    std::string val;
    while (p < json.size() && json[p] != '"') {
        if (json[p] == '\\' && p + 1 < json.size()) {
            p++;
            if      (json[p] == '"')  val += '"';
            else if (json[p] == '\\') val += '\\';
            else if (json[p] == 'n')  val += '\n';
            else                      val += json[p];
        } else {
            val += json[p];
        }
        p++;
    }
    return val;
}

// Extrae valor numerico de "clave":123 en JSON ya aplanado
inline std::string getNumber(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\":";
    auto p = json.find(pat);
    if (p == std::string::npos) return "";
    p += pat.size();
    auto e = p;
    while (e < json.size() && (isdigit((unsigned char)json[e]) || json[e] == '.' || json[e] == '-'))
        e++;
    return json.substr(p, e - p);
}

// Extrae valor booleano de "clave":true/false en JSON ya aplanado
inline bool getBool(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\":";
    auto p = json.find(pat);
    if (p == std::string::npos) return false;
    p += pat.size();
    return json.substr(p, 4) == "true";
}

// Divide un array JSON en objetos individuales (maneja anidamiento)
inline std::vector<std::string> splitArray(const std::string& jsonRaw) {
    std::string json = aplanar(jsonRaw);
    std::vector<std::string> objs;
    int depth = 0;
    int start = -1;
    bool inStr = false;
    for (size_t i = 0; i < json.size(); i++) {
        char c = json[i];
        if (c == '"' && (i == 0 || json[i-1] != '\\')) inStr = !inStr;
        if (!inStr) {
            if (c == '{') { if (depth++ == 0) start = (int)i; }
            else if (c == '}') {
                if (--depth == 0 && start != -1) {
                    objs.push_back(json.substr(start, i - start + 1));
                    start = -1;
                }
            }
        }
    }
    return objs;
}

// Convierte Paciente → JSON string
inline std::string toJson(const Paciente& p) {
    std::string algs = "[";
    for (size_t i = 0; i < p.alergias.size(); i++) {
        if (i) algs += ",";
        algs += "\"" + escapar(p.alergias[i]) + "\"";
    }
    algs += "]";
    std::ostringstream o;
    o << "{\"idPaciente\":\"" << escapar(p.idPaciente) << "\","
      << "\"nombre\":\""      << escapar(p.nombre)     << "\","
      << "\"tipoSangre\":\""  << escapar(p.tipoSangre) << "\","
      << "\"alergias\":"      << algs                   << ","
      << "\"nivelTriage\":"   << p.nivelTriage          << ","
      << "\"scoreSOFA\":"     << p.scoreSOFA            << ","
      << "\"enUCI\":"         << (p.enUCI ? "true" : "false") << ","
      << "\"edad\":"          << p.edad                 << ","
      << "\"diagnostico\":\"" << escapar(p.diagnostico) << "\","
      << "\"sofaMedido\":"    << (p.sofaParams.medido ? "true" : "false") << ","
      << "\"pao2fio2\":"      << p.sofaParams.pao2fio2    << ","
      << "\"ventilado\":"     << (p.sofaParams.ventilado ? "true" : "false") << ","
      << "\"plaquetas\":"     << p.sofaParams.plaquetas   << ","
      << "\"bilirrubina\":"   << p.sofaParams.bilirrubina << ","
      << "\"pam\":"           << p.sofaParams.pam         << ","
      << "\"vasopresor\":"    << p.sofaParams.vasopresor  << ","
      << "\"glasgow\":"       << p.sofaParams.glasgow     << ","
      << "\"creatinina\":"    << p.sofaParams.creatinina  << "}";
    return o.str();
}

// Lee un numero opcional; devuelve 'def' si la clave no existe
inline double numOpt(const std::string& j, const std::string& key, double def) {
    std::string s = getNumber(j, key);
    if (s.empty()) return def;
    try { return std::stod(s); } catch (...) { return def; }
}

// Convierte JSON objeto → Paciente
inline Paciente toPaciente(const std::string& jsonRaw) {
    std::string j = aplanar(jsonRaw);
    Paciente p;
    p.idPaciente = getString(j, "idPaciente");
    p.nombre     = getString(j, "nombre");
    p.tipoSangre = getString(j, "tipoSangre");

    // _id puede ser {"$oid":"..."} o string directo
    auto oidp = j.find("\"$oid\":\"");
    if (oidp != std::string::npos) {
        oidp += 8;
        auto oidEnd = j.find('"', oidp);
        p.mongoId = j.substr(oidp, oidEnd - oidp);
    } else {
        p.mongoId = getString(j, "_id");
    }

    std::string triage = getNumber(j, "nivelTriage");
    std::string sofa   = getNumber(j, "scoreSOFA");
    p.enUCI       = getBool(j, "enUCI");

    try { p.nivelTriage = triage.empty() ? 5   : std::stoi(triage); } catch(...) { p.nivelTriage = 5; }
    try { p.scoreSOFA   = sofa.empty()   ? 0.0 : std::stod(sofa);   } catch(...) { p.scoreSOFA   = 0; }

    // Campos opcionales (registros antiguos no los tienen)
    p.edad        = (int)numOpt(j, "edad", 0);
    p.diagnostico = getString(j, "diagnostico");
    p.sofaParams.medido      = getBool(j, "sofaMedido");
    p.sofaParams.pao2fio2    = numOpt(j, "pao2fio2",    400);
    p.sofaParams.ventilado   = getBool(j, "ventilado");
    p.sofaParams.plaquetas   = numOpt(j, "plaquetas",   200);
    p.sofaParams.bilirrubina = numOpt(j, "bilirrubina", 0.5);
    p.sofaParams.pam         = numOpt(j, "pam",         80);
    p.sofaParams.vasopresor  = (int)numOpt(j, "vasopresor", 0);
    p.sofaParams.glasgow     = (int)numOpt(j, "glasgow",    15);
    p.sofaParams.creatinina  = numOpt(j, "creatinina",  0.8);
    p.recalcularSOFA();

    // Alergias: "alergias":["a","b"]
    auto ap = j.find("\"alergias\":[");
    if (ap != std::string::npos) {
        ap += 12;
        auto ae = j.find(']', ap);
        std::string arr = j.substr(ap, ae == std::string::npos ? std::string::npos : ae - ap);
        size_t i = 0;
        while (i < arr.size()) {
            if (arr[i] == '"') {
                std::string v; i++;
                while (i < arr.size() && arr[i] != '"') {
                    if (arr[i] == '\\' && i + 1 < arr.size()) i++;
                    v += arr[i]; i++;
                }
                if (!v.empty()) p.alergias.push_back(v);
            }
            i++;
        }
    }
    return p;
}

} // namespace JSON

// ================================================================
//  SECCION 2: HTTP via WinHTTP
// ================================================================
namespace HTTP {

struct Respuesta {
    long        codigo = 0;
    std::string cuerpo;
    bool ok() const { return codigo >= 200 && codigo < 300; }
};

inline std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

inline Respuesta request(const std::string& url,
                          const std::string& method,
                          const std::string& body = "") {
    Respuesta resp;

    // Parsear URL
    bool https = url.substr(0, 8) == "https://";
    size_t off = https ? 8 : 7;
    size_t sl  = url.find('/', off);
    std::wstring host = toWide(sl == std::string::npos ? url.substr(off) : url.substr(off, sl - off));
    std::wstring path = sl == std::string::npos ? L"/" : toWide(url.substr(sl));

    HINTERNET hSess = WinHttpOpen(L"EHR/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return resp;
    // Timeouts (ms): resolver, conectar, enviar, recibir
    WinHttpSetTimeouts(hSess, 5000, 5000, 10000, 15000);

    HINTERNET hConn = WinHttpConnect(hSess, host.c_str(),
        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return resp; }

    std::wstring wmethod = toWide(method);
    HINTERNET hReq = WinHttpOpenRequest(hConn, wmethod.c_str(), path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        https ? WINHTTP_FLAG_SECURE : 0);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return resp; }

    // Ignorar errores de certificado en desarrollo
    DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                  SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                  SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                  SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
    WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));

    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json";
    BOOL sent;
    if (!body.empty()) {
        sent = WinHttpSendRequest(hReq, headers.c_str(), (DWORD)-1,
            (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    } else {
        sent = WinHttpSendRequest(hReq, headers.c_str(), (DWORD)-1,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }

    if (!sent || !WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
        return resp;
    }

    DWORD statusSize = sizeof(DWORD), status = 0;
    WinHttpQueryHeaders(hReq,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    resp.codigo = (long)status;

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
        std::string buf(avail, '\0');
        DWORD read = 0;
        WinHttpReadData(hReq, &buf[0], avail, &read);
        resp.cuerpo.append(buf, 0, read);
    }

    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
    return resp;
}

} // namespace HTTP

// ================================================================
//  SECCION 3: Clase MongoAPI
// ================================================================
class MongoAPI {
public:
    static const std::string COLECCION;
    static const std::string URL;

private:
    bool verbose;

    HTTP::Respuesta call(const std::string& url,
                         const std::string& method,
                         const std::string& body = "") {
        ULONGLONG t0 = GetTickCount64();
        auto r = HTTP::request(url, method, body);
        ultimoMetodo = method;
        ultimoCodigo = r.codigo;
        ultimoMs     = (long)(GetTickCount64() - t0);
        if (verbose)
            std::cout << "  [API] " << method << " HTTP " << r.codigo << "\n";
        return r;
    }

public:
    // Datos de la ultima llamada HTTP (para el log de sincronizacion)
    std::string ultimoMetodo;
    long        ultimoCodigo = 0;   // 0 = sin respuesta (error de red)
    long        ultimoMs     = 0;

    explicit MongoAPI(bool v = false) : verbose(v) {}

    // POST: insertar
    std::string insertar(const Paciente& p) {
        std::string payload = "{\"coleccion\":\"" + COLECCION + "\","
                              "\"datos\":" + JSON::toJson(p) + "}";
        auto r = call(URL, "POST", payload);
        if (!r.ok()) return "";
        return JSON::getString(JSON::aplanar(r.cuerpo), "_id");
    }

    // GET: consultar todos → extrae array "documentos"
    std::vector<Paciente> consultarTodos() {
        auto r = call(URL + "?coleccion=" + COLECCION, "GET");
        std::vector<Paciente> lista;
        if (!r.ok()) return lista;

        std::string flat = JSON::aplanar(r.cuerpo);

        // Extraer el array "documentos":[...]
        std::string pat = "\"documentos\":[";
        auto pos = flat.find(pat);
        if (pos == std::string::npos) return lista;
        pos += pat.size() - 1; // apuntar al '['

        // Balancear corchetes para encontrar el cierre
        int depth = 0;
        size_t arrEnd = pos;
        for (size_t i = pos; i < flat.size(); i++) {
            if      (flat[i] == '[') depth++;
            else if (flat[i] == ']') { if (--depth == 0) { arrEnd = i; break; } }
        }
        std::string arr = flat.substr(pos, arrEnd - pos + 1);

        for (const auto& obj : JSON::splitArray(arr)) {
            Paciente p = JSON::toPaciente(obj);
            if (verbose)
                std::cout << "  [API] Parseado: '" << p.idPaciente
                          << "' / '" << p.nombre << "'\n";
            lista.push_back(p);
        }
        return lista;
    }

    // PUT: actualizar
    bool actualizar(const Paciente& p) {
        std::string payload = "{\"coleccion\":\"" + COLECCION + "\","
            "\"filtro\":{\"idPaciente\":\"" + JSON::escapar(p.idPaciente) + "\"},"
            "\"datos\":" + JSON::toJson(p) + "}";
        return call(URL, "PUT", payload).ok();
    }

    // DELETE: eliminar
    bool eliminar(const std::string& id) {
        std::string payload = "{\"coleccion\":\"" + COLECCION + "\","
            "\"filtro\":{\"idPaciente\":\"" + JSON::escapar(id) + "\"}}";
        return call(URL, "DELETE", payload).ok();
    }

    // Verificar conexion
    bool estaConectado() {
        auto r = call(URL + "?coleccion=" + COLECCION, "GET");
        return r.ok() && r.cuerpo.find("documentos") != std::string::npos;
    }
};

const std::string MongoAPI::COLECCION = "pacientes_ehr";
const std::string MongoAPI::URL       = "https://paginas-web-cr.com/Api/apis/mongodb.php";
