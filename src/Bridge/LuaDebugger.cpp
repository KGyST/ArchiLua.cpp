// LuaDebugger.cpp — Minimal DAP server for debugging Lua scripts in ArchiCAD
//
// Define Windows-lean macros BEFORE any includes so STL headers
// and windows.h both see them.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "LuaDebugger.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <set>
#include <mutex>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include "LuaDebugger.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <set>
#include <mutex>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include "APIEnvir.h"
#include "ACAPinc.h"

namespace ArchiLua {

// =========================================================================
//  JSON parser / builder
// =========================================================================

void Json::skipWS(const char*& p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        ++p;
}

Json Json::parseStr(const char*& p) {
    Json v;
    v.type = Str;
    ++p; // skip opening "
    while (*p && *p != '"') {
        if (*p == '\\') {
            ++p;
            switch (*p) {
                case '"':  v.s += '"';  break;
                case '\\': v.s += '\\'; break;
                case '/':  v.s += '/';  break;
                case 'b':  v.s += '\b'; break;
                case 'f':  v.s += '\f'; break;
                case 'n':  v.s += '\n'; break;
                case 'r':  v.s += '\r'; break;
                case 't':  v.s += '\t'; break;
                case 'u': {
                    char hex[5] = {};
                    for (int i = 0; i < 4; ++i) hex[i] = *++p;
                    unsigned int cp = (unsigned int)std::strtoul(hex, nullptr, 16);
                    if (cp < 128) v.s += (char)cp;
                    else          v.s += '?';
                    break;
                }
                default: v.s += *p; break;
            }
        } else {
            v.s += *p;
        }
        ++p;
    }
    if (*p == '"') ++p;
    return v;
}

Json Json::parseNum(const char*& p) {
    Json v;
    v.type = Num;
    const char* start = p;
    if (*p == '-') ++p;
    while (*p >= '0' && *p <= '9') ++p;
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') ++p;
    }
    if (*p == 'e' || *p == 'E') {
        ++p;
        if (*p == '+' || *p == '-') ++p;
        while (*p >= '0' && *p <= '9') ++p;
    }
    v.n = std::strtod(start, nullptr);
    return v;
}

Json Json::parseVal(const char*& p) {
    skipWS(p);
    if (!*p) return Json{};
    if (*p == '"')   return parseStr(p);
    if (*p == '{') {
        Json v; v.type = Obj;
        ++p; skipWS(p);
        if (*p == '}') { ++p; return v; }
        while (true) {
            skipWS(p);
            if (*p != '"') break;
            auto key = parseStr(p);
            skipWS(p);
            if (*p == ':') { ++p; }
            auto val = parseVal(p);
            v.o.emplace_back(key.s, val);
            skipWS(p);
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; break; }
            break;
        }
        return v;
    }
    if (*p == '[') {
        Json v; v.type = Arr;
        ++p; skipWS(p);
        if (*p == ']') { ++p; return v; }
        while (true) {
            v.a.push_back(parseVal(p));
            skipWS(p);
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; break; }
            break;
        }
        return v;
    }
    if (*p == 't') { p += 4; Json v; v.type = Bool; v.b = true;  return v; }
    if (*p == 'f') { p += 5; Json v; v.type = Bool; v.b = false; return v; }
    if (*p == 'n') { p += 4; return Json{}; }
    return parseNum(p);
}

Json Json::Parse(const char*& p) {
    return parseVal(p);
}

std::string Json::escape(const std::string& raw) {
    std::string r;
    for (char c : raw) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\b': r += "\\b";  break;
            case '\f': r += "\\f";  break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if ((unsigned char)c < 32) {
                    char buf[8]; std::sprintf(buf, "\\u%04x", (unsigned char)c);
                    r += buf;
                } else {
                    r += c;
                }
        }
    }
    return r;
}

std::string Json::Dump() const {
    switch (type) {
        case Null: return "null";
        case Bool: return b ? "true" : "false";
        case Num:  return std::to_string(n);
        case Str:  return "\"" + escape(s) + "\"";
        case Arr: {
            std::string r = "[";
            for (size_t i = 0; i < a.size(); ++i) {
                if (i) r += ",";
                r += a[i].Dump();
            }
            return r + "]";
        }
        case Obj: {
            std::string r = "{";
            for (size_t i = 0; i < o.size(); ++i) {
                if (i) r += ",";
                r += "\"" + escape(o[i].first) + "\":" + o[i].second.Dump();
            }
            return r + "}";
        }
    }
    return "null";
}

const Json& Json::get(const char* key) const {
    static Json nullJson;
    if (type != Obj) return nullJson;
    for (auto& p : o)
        if (p.first == key) return p.second;
    return nullJson;
}

// =========================================================================
//  JSON string helpers (for building DAP responses)
// =========================================================================
namespace {

std::string jsonStr(const std::string& raw) {
    return "\"" + Json::escape(raw) + "\"";
}

} // anonymous namespace

// =========================================================================
//  DAP wire format helpers
// =========================================================================

static std::string DapEncode(const std::string& body) {
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

static bool DapDecode(const std::string& buf, std::string& body) {
    auto pos = buf.find("\r\n\r\n");
    if (pos == std::string::npos) return false;
    auto hdr = buf.substr(0, pos);
    auto clpos = hdr.find("Content-Length:");
    if (clpos == std::string::npos) return false;
    int len = std::atoi(hdr.c_str() + clpos + 15);
    if ((int)buf.size() < pos + 4 + len) return false;
    body = buf.substr(pos + 4, len);
    return true;
}

// =========================================================================
//  TCP / Winsock helpers
// =========================================================================

static bool WsaStartup() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

static int CreateListener(int port) {
    int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((unsigned short)port);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(sock);
        return -1;
    }
    if (listen(sock, 1) < 0) {
        closesocket(sock);
        return -1;
    }
    return sock;
}

static int AcceptClient(int listenSock) {
    sockaddr_in client = {};
    int sz = sizeof(client);
    return (int)accept(listenSock, (sockaddr*)&client, &sz);
}

static int RecvSome(int sock, std::string& buf) {
    char tmp[4096];
    int n = recv(sock, tmp, sizeof(tmp), 0);
    if (n > 0) buf.append(tmp, n);
    return n;
}

static int SendAll(int sock, const std::string& data) {
    int total = 0;
    while (total < (int)data.size()) {
        int n = send(sock, data.c_str() + total, (int)data.size() - total, 0);
        if (n <= 0) return n;
        total += n;
    }
    return total;
}

// =========================================================================
//  LuaDebugger implementation
// =========================================================================

// File-scope pointer: Lua 5.4's debug hook has no userdata parameter,
// so we store the active debugger here. Safe because we only have one
// Lua state running on the main thread.
static LuaDebugger* s_activeDebugger = nullptr;

void LuaDebugger::NotifyRunStarting() {
    s_activeDebugger = this;
    if (m_connected) {
        m_pauseReq = 1;
    }
}

void LuaDebugger::NotifyRunEnded() {
    s_activeDebugger = nullptr;
    if (m_connected) {
        m_state = State::Idle;
    }
}

bool LuaDebugger::Start(lua_State* L, int port) {
    if (m_active) return true;
    m_L = L;
    if (!WsaStartup()) return false;
    m_listenSock = CreateListener(port);
    if (m_listenSock < 0) return false;

    m_wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_wakeEvent) { closesocket(m_listenSock); return false; }

    m_active = true;
    m_serverThread = std::thread([this]() { ServerThread(); });
    m_serverThread.detach();
    return true;
}

void LuaDebugger::Stop() {
    if (!m_active) return;
    m_active = false;
    m_connected = false;
    s_activeDebugger = nullptr;

    if (m_clientSock >= 0) {
        closesocket(m_clientSock);
        m_clientSock = -1;
    }
    if (m_listenSock >= 0) {
        closesocket(m_listenSock);
        m_listenSock = -1;
    }
    if (m_wakeEvent) {
        SetEvent(m_wakeEvent);
        CloseHandle(m_wakeEvent);
        m_wakeEvent = nullptr;
    }
    WSACleanup();
}

void LuaDebugger::SendOutput(const char* text) {
    if (!m_connected) return;
    std::string body = "\"category\":\"stdout\",\"output\":" + jsonStr(text);
    SendEvent("output", body);
}

// =========================================================================
//  Server thread — blocks on accept(), then reads DAP messages
// =========================================================================

void LuaDebugger::ServerThread() {
    while (m_active) {
        int client = AcceptClient(m_listenSock);
        if (client < 0 || !m_active) break;

        m_clientSock = client;
        m_connected = true;
        m_state = State::Attached;

        std::string buf;
        std::string body;

        while (m_active && m_connected) {
            int n = RecvSome(client, buf);
            if (n <= 0) break;

            while (!buf.empty() && DapDecode(buf, body)) {
                // Remove processed message from buffer
                auto pos = buf.find("\r\n\r\n");
                auto clpos = buf.find("Content-Length:");
                int len = 0;
                if (clpos != std::string::npos)
                    len = std::atoi(buf.c_str() + clpos + 15);
                buf.erase(0, pos + 4 + len);

                // Queue the request for the main thread
                {
                    std::lock_guard<std::mutex> lock(m_inMutex);
                    m_inQueue.push_back(body);
                }
                if (m_wakeEvent)
                    SetEvent(m_wakeEvent);
            }
        }

        closesocket(client);
        m_clientSock = -1;
        m_connected = false;
        m_state = State::Idle;
    }
}

// =========================================================================
//  Process a single DAP request (called from main thread)
// =========================================================================

bool LuaDebugger::ProcessRequest(const std::string& raw) {
    auto req = Json::Parse(raw);

    int seq = req.get("seq").asInt();
    std::string cmd = req.get("command").asStr();
    auto args = req.get("arguments");

    if (cmd == "initialize")       OnInitialize(seq, args);
    else if (cmd == "attach")      OnAttach(seq, args);
    else if (cmd == "launch")      OnAttach(seq, args);
    else if (cmd == "setBreakpoints")       OnSetBreakpoints(seq, args);
    else if (cmd == "setFunctionBreakpoints") { /* no-op, respond success */ SendResponse(seq, cmd); }
    else if (cmd == "configurationDone")    OnConfigurationDone(seq, args);
    else if (cmd == "threads")              OnThreads(seq, args);
    else if (cmd == "stackTrace")           OnStackTrace(seq, args);
    else if (cmd == "scopes")               OnScopes(seq, args);
    else if (cmd == "variables")            OnVariables(seq, args);
    else if (cmd == "continue")             OnContinue(seq, args);
    else if (cmd == "next")                 OnNext(seq, args);
    else if (cmd == "stepIn")               OnStepIn(seq, args);
    else if (cmd == "stepOut")              OnStepOut(seq, args);
    else if (cmd == "pause")                OnPause(seq, args);
    else if (cmd == "disconnect")           OnDisconnect(seq, args);
    else {
        std::string err = "{\"seq\":" + std::to_string(m_seq++) +
            ",\"type\":\"response\",\"request_seq\":" + std::to_string(seq) +
            ",\"command\":\"" + cmd + "\",\"success\":false,\"message\":\"not implemented\"}";
        Send(err);
    }
    return true;
}

// =========================================================================
//  Lua debug hook — runs on main thread during script execution
// =========================================================================

void LuaDebugger::DebugHook(lua_State* L, lua_Debug* ar) {
    auto* self = s_activeDebugger;
    if (!self || !self->m_connected) return;

    int curLine = -1;
    const char* src = "";

    if (lua_getinfo(L, "Sl", ar)) {
        curLine = ar->currentline;
        if (ar->source && ar->source[0] == '@')
            src = ar->source + 1;
        else if (ar->source)
            src = ar->source;
    }

    // Process any queued requests
    {
        std::lock_guard<std::mutex> lock(self->m_inMutex);
        for (size_t i = 0; i < self->m_inQueue.size(); ++i) {
            self->ProcessRequest(self->m_inQueue[i]);
        }
        self->m_inQueue.clear();
    }

    // Check if we should pause
    bool hit = false;

    if (self->m_pauseReq) {
        self->m_pauseReq = 0;
        hit = true;
    }
    else if (self->m_state == State::Stepping) {
        hit = true;
    }
    else if (self->m_state == State::Next) {
        int depth = 0;
        lua_Debug d;
        while (lua_getstack(L, depth, &d)) ++depth;
        if (depth <= self->m_targetDepth) hit = true;
    }
    else if (self->m_state == State::StepOut) {
        int depth = 0;
        lua_Debug d;
        while (lua_getstack(L, depth, &d)) ++depth;
        if (depth < self->m_targetDepth) hit = true;
    }
    else if (curLine > 0 && src[0]) {
        if (self->HitBreakpoint(src, curLine))
            hit = true;
    }

    if (!hit) return;

    // Pause — enter debug loop
    self->m_state = State::Paused;
    self->SendStopped("breakpoint");

    while (self->m_active && self->m_state == State::Paused) {
        // Pump Windows messages to keep ArchiCAD responsive
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Process queued requests
        bool didWork = false;
        {
            std::lock_guard<std::mutex> lock(self->m_inMutex);
            for (size_t i = 0; i < self->m_inQueue.size(); ++i) {
                self->ProcessRequest(self->m_inQueue[i]);
                didWork = true;
            }
            self->m_inQueue.clear();
        }

        if (!didWork) {
            WaitForSingleObject(self->m_wakeEvent, 50);
        }
    }
}

// =========================================================================
//  Send helpers
// =========================================================================

void LuaDebugger::Send(const std::string& body) {
    if (m_clientSock < 0) return;
    auto wire = DapEncode(body);
    SendAll(m_clientSock, wire);
}

void LuaDebugger::SendEvent(const std::string& name, const std::string& body) {
    std::string msg = "{\"seq\":" + std::to_string(m_seq++) +
        ",\"type\":\"event\",\"event\":\"" + name + "\",\"body\":{" + body + "}}";
    Send(msg);
}

void LuaDebugger::SendResponse(int seq, const std::string& cmd, const std::string& body) {
    std::string msg = "{\"seq\":" + std::to_string(m_seq++) +
        ",\"type\":\"response\",\"request_seq\":" + std::to_string(seq) +
        ",\"command\":\"" + cmd + "\",\"success\":true";
    if (!body.empty())
        msg += ",\"body\":{" + body + "}";
    msg += "}";
    Send(msg);
}

void LuaDebugger::SendStopped(const char* reason) {
    SendEvent("stopped", "\"reason\":\"" + std::string(reason) + "\",\"threadId\":1");
}

// =========================================================================
//  DAP request handlers
// =========================================================================

void LuaDebugger::OnInitialize(int seq, const Json&) {
    SendResponse(seq, "initialize",
        "\"supportsConfigurationDoneRequest\":true"
        ",\"supportsConditionalBreakpoints\":false"
        ",\"supportsFunctionBreakpoints\":false"
        ",\"supportsSetVariable\":false"
        ",\"supportsStepInTargetsRequest\":false"
        ",\"supportsStepping\":true"
        ",\"supportsTerminateRequest\":true"
    );
    SendEvent("initialized", "");
}

void LuaDebugger::OnAttach(int seq, const Json&) {
    SendResponse(seq, "attach");
}

void LuaDebugger::OnSetBreakpoints(int seq, const Json& args) {
    auto src = args.get("source");
    auto bpList = args.get("breakpoints");
    std::string path = src.get("path").asStr();
    if (path.empty()) path = src.get("name").asStr();

    m_breakpoints.erase(path);
    std::vector<int> lines;

    if (bpList.isArr()) {
        for (auto& bp : bpList.a) {
            int ln = bp.get("line").asInt();
            lines.push_back(ln);
        }
    }
    m_breakpoints[path] = lines;

    // Build response
    std::string bps;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) bps += ",";
        bps += "{\"id\":" + std::to_string(i + 1) + ",\"line\":" + std::to_string(lines[i]) + ",\"verified\":true}";
    }
    SendResponse(seq, "setBreakpoints", "\"breakpoints\":[" + bps + "]");
}

void LuaDebugger::OnConfigurationDone(int seq, const Json&) {
    SendResponse(seq, "configurationDone");
}

void LuaDebugger::OnThreads(int seq, const Json&) {
    SendResponse(seq, "threads", "\"threads\":[{\"id\":1,\"name\":\"Main\"}]");
}

void LuaDebugger::OnStackTrace(int seq, const Json&) {
    std::string frames;
    int depth = 0;
    lua_Debug ar;
    while (lua_getstack(m_L, depth, &ar)) {
        if (depth > 0) frames += ",";
        lua_getinfo(m_L, "Sln", &ar);

        const char* src = ar.source;
        if (src && src[0] == '@') ++src;

        int ref = m_varRef++;

        frames += "{\"id\":" + std::to_string(depth) +
            ",\"name\":" + jsonStr(ar.name ? ar.name : "<anonymous>") +
            ",\"line\":" + std::to_string(ar.currentline) +
            ",\"column\":0" +
            ",\"source\":{\"path\":" + jsonStr(src ? src : "?") +
            ",\"sourceReference\":" + std::to_string(ref) + "}}";
        ++depth;
    }
    SendResponse(seq, "stackTrace", "\"stackFrames\":[" + frames + "],\"totalFrames\":" + std::to_string(depth));
}

void LuaDebugger::OnScopes(int seq, const Json& args) {
    int frameId = args.get("frameId").asInt();
    int ref = m_varRef++;
    std::string scopes =
        "{\"name\":" + jsonStr("Locals") + ",\"variablesReference\":" + std::to_string(ref) + ",\"expensive\":false}";

    if (frameId >= 0) {
        lua_Debug ar;
        if (lua_getstack(m_L, frameId, &ar)) {
            lua_getinfo(m_L, "f", &ar);
            if (lua_getupvalue(m_L, -1, 1) != nullptr) {
                lua_pop(m_L, 1);
                int uref = m_varRef++;
                scopes += ",{\"name\":" + jsonStr("Upvalues") + ",\"variablesReference\":" + std::to_string(uref) + ",\"expensive\":false}";
            }
            lua_pop(m_L, 1);
        }
    }

    SendResponse(seq, "scopes", "\"scopes\":[" + scopes + "]");
}

void LuaDebugger::OnVariables(int seq, const Json&) {
    std::string vars;
    lua_Debug ar;

    if (lua_getstack(m_L, 0, &ar)) {
        lua_getinfo(m_L, "f", &ar);

        // Enumerate locals
        int i = 1;
        const char* name;
        while ((name = lua_getlocal(m_L, &ar, i)) != nullptr) {
            if (vars.size()) vars += ",";
            int t = lua_type(m_L, -1);
            const char* typeName = lua_typename(m_L, t);
            std::string val;

            switch (t) {
                case LUA_TNIL:      val = "null";    break;
                case LUA_TBOOLEAN:  val = lua_toboolean(m_L, -1) ? "true" : "false"; break;
                case LUA_TNUMBER:   val = std::to_string(lua_tonumber(m_L, -1)); break;
                case LUA_TSTRING:   val = lua_tostring(m_L, -1); break;
                case LUA_TTABLE:    val = "{table}"; break;
                case LUA_TFUNCTION: val = "function"; break;
                default:            val = lua_typename(m_L, t); break;
            }

            vars += "{\"name\":" + jsonStr(name) +
                    ",\"value\":" + jsonStr(val) +
                    ",\"type\":" + jsonStr(typeName) +
                    ",\"variablesReference\":0}";
            lua_pop(m_L, 1);
            ++i;
        }
        lua_pop(m_L, 1);

        // Enumerate upvalues
        i = 1;
        while ((name = lua_getupvalue(m_L, -1, i)) != nullptr) {
            int t = lua_type(m_L, -1);
            const char* typeName = lua_typename(m_L, t);
            std::string val;

            switch (t) {
                case LUA_TNIL:      val = "null";    break;
                case LUA_TBOOLEAN:  val = lua_toboolean(m_L, -1) ? "true" : "false"; break;
                case LUA_TNUMBER:   val = std::to_string(lua_tonumber(m_L, -1)); break;
                case LUA_TSTRING:   val = lua_tostring(m_L, -1); break;
                case LUA_TTABLE:    val = "{table}"; break;
                case LUA_TFUNCTION: val = "function"; break;
                default:            val = lua_typename(m_L, t); break;
            }

            vars += ",{\"name\":" + jsonStr(name) +
                    ",\"value\":" + jsonStr(val) +
                    ",\"type\":" + jsonStr(typeName) +
                    ",\"variablesReference\":0}";
            lua_pop(m_L, 1);
            ++i;
        }
    }

    SendResponse(seq, "variables", "\"variables\":[" + vars + "]");
}

void LuaDebugger::OnContinue(int seq, const Json&) {
    SendResponse(seq, "continue", "\"allThreadsContinued\":true");
    m_state = State::Running;
}

void LuaDebugger::OnNext(int seq, const Json&) {
    SendResponse(seq, "next");
    m_state = State::Next;

    lua_Debug d;
    m_targetDepth = 0;
    while (lua_getstack(m_L, m_targetDepth, &d)) ++m_targetDepth;
}

void LuaDebugger::OnStepIn(int seq, const Json&) {
    SendResponse(seq, "stepIn");
    m_state = State::Stepping;
}

void LuaDebugger::OnStepOut(int seq, const Json&) {
    SendResponse(seq, "stepOut");
    m_state = State::StepOut;

    lua_Debug d;
    m_targetDepth = 0;
    while (lua_getstack(m_L, m_targetDepth, &d)) ++m_targetDepth;
}

void LuaDebugger::OnPause(int seq, const Json&) {
    SendResponse(seq, "pause");
    m_pauseReq = 1;
}

void LuaDebugger::OnDisconnect(int seq, const Json&) {
    SendResponse(seq, "disconnect");
    m_connected = false;
    m_state = State::Disco;
}

// =========================================================================
//  Breakpoint matching
// =========================================================================

bool LuaDebugger::HitBreakpoint(const char* source, int line) {
    if (!source || !*source) return false;

    // Try exact path match first
    auto it = m_breakpoints.find(source);
    if (it != m_breakpoints.end()) {
        for (int bp : it->second)
            if (bp == line) return true;
        return false;
    }

    // Try filename-only match (strip directory)
    std::string fname;
    const char* slash = std::strrchr(source, '\\');
    if (!slash) slash = std::strrchr(source, '/');
    if (slash) fname = slash + 1; else fname = source;

    for (auto& kv : m_breakpoints) {
        std::string keyFname;
        slash = std::strrchr(kv.first.c_str(), '\\');
        if (!slash) slash = std::strrchr(kv.first.c_str(), '/');
        if (slash) keyFname = slash + 1; else keyFname = kv.first;

        if (keyFname == fname) {
            for (int bp : kv.second)
                if (bp == line) return true;
        }
    }

    return false;
}

} // namespace ArchiLua
