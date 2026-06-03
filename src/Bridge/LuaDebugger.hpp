#pragma once

extern "C" {
#include <lua.h>
}

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>

namespace ArchiLua {

// ---------------------------------------------------------------------------
// Minimal JSON value — recursive descent parser + string builder
// ---------------------------------------------------------------------------
struct Json {
    enum Type { Null, Bool, Num, Str, Arr, Obj };
    Type        type = Null;
    bool        b   = false;
    double      n   = 0.0;
    std::string s;
    std::vector<Json>          a;
    std::vector<std::pair<std::string, Json>> o;

    static Json Parse(const char*& p);
    static Json Parse(const std::string& src) { const char* p = src.c_str(); return Parse(p); }

    std::string Dump() const;

    const Json& get(const char* key) const;
    const Json& operator[](size_t i) const { return a[i]; }
    int         asInt()              const { return (int)n; }
    const std::string& asStr()       const { return s; }
    bool        isArr()              const { return type == Arr; }

    static std::string escape(const std::string& raw);

private:
    static void skipWS(const char*& p);
    static Json parseVal(const char*& p);
    static Json parseStr(const char*& p);
    static Json parseNum(const char*& p);
};

// ---------------------------------------------------------------------------
// Debug Adapter Protocol server
// ---------------------------------------------------------------------------
class LuaDebugger {
public:
    LuaDebugger() = default;
    ~LuaDebugger() { Stop(); }

    LuaDebugger(const LuaDebugger&) = delete;
    LuaDebugger& operator=(const LuaDebugger&) = delete;

    bool Start(lua_State* L, int port = 4711);
    void Stop();
    bool IsActive()    const { return m_active; }
    bool HasClient()   const { return m_connected; }

    // Called by Bridge before/after executing a script
    void NotifyRunStarting();
    void NotifyRunEnded();

    // Send output event (called from LuaConsole)
    void SendOutput(const char* text);

    // Lua debug hook (static, registered via lua_sethook by Bridge)
    static void DebugHook(lua_State* L, lua_Debug* ar);

private:
    // Background thread: accept + read DAP messages
    void ServerThread();

    // Main-thread: process a single DAP request (called from hook loop)
    bool ProcessRequest(const std::string& raw);

    // Main-thread: write response/event to client socket
    void Send(const std::string& body);

    // --- DAP request handlers (all called on main thread) ---
    void OnInitialize(int seq, const Json& args);
    void OnAttach(int seq, const Json& args);
    void OnSetBreakpoints(int seq, const Json& args);
    void OnConfigurationDone(int seq, const Json& args);
    void OnThreads(int seq, const Json& args);
    void OnStackTrace(int seq, const Json& args);
    void OnScopes(int seq, const Json& args);
    void OnVariables(int seq, const Json& args);
    void OnContinue(int seq, const Json& args);
    void OnNext(int seq, const Json& args);
    void OnStepIn(int seq, const Json& args);
    void OnStepOut(int seq, const Json& args);
    void OnPause(int seq, const Json& args);
    void OnDisconnect(int seq, const Json& args);

    // Helpers
    void SendEvent(const std::string& name, const std::string& body);
    void SendStopped(const char* reason);
    void SendResponse(int seq, const std::string& cmd, const std::string& body = "");

    // Breakpoint match
    bool HitBreakpoint(const char* source, int line);

    // State
    enum class State { Idle, Attached, Running, Paused, Stepping, Next, StepOut, Disco };
    State m_state = State::Idle;

    // --- Threading ---
    std::atomic<bool> m_active     { false };
    std::atomic<bool> m_connected  { false };
    std::thread       m_serverThread;
    // Synchronisation for incoming data (producer: server thread, consumer: hook)
    std::mutex                    m_inMutex;
    std::vector<std::string>      m_inQueue;
    // The server thread signals the hook loop with this event
    void* m_wakeEvent = nullptr;

    // Socket handles
    int m_listenSock = -1;
    int m_clientSock = -1;

    // Lua state (raw ptr, not owned)
    lua_State* m_L = nullptr;

    // DAP sequence number
    int m_seq = 1;

    // Breakpoints: source path → set of lines
    std::map<std::string, std::vector<int>> m_breakpoints;

    // Stepping state
    int m_targetDepth = 0;
    int m_pauseReq    = 0;

    // Variable reference counter
    int m_varRef = 1000;
};

} // namespace ArchiLua
