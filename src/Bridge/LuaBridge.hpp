#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <string>

#include "../Console/LuaConsole.hpp"
#include "../API/APIModule.hpp"
#include "../ArchiLua.hpp"
#include "LuaDebugger.hpp"

namespace ArchiLua {

class Bridge {
public:
    Bridge() { LoadLastScriptPath(); }
    ~Bridge() { Shutdown(); }

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    void Init()
    {
        if (L)
            return;
        L = luaL_newstate();
        luaL_openlibs(L);

        // Start DAP server (always listening, connects on demand)
        m_debugger.Start(L);

        // Pass a callback so Lua print() also forwards to DAP
        LuaConsole::Register(L, [](const char* s) {
            Bridge& b = ArchiLua::GetBridge();
            if (b.m_debugger.HasClient())
                b.m_debugger.SendOutput(s);
        });

        APIModule::Register(L);

        LoadLastScriptPath();
    }

    void ExecuteScript(const std::string& path)
    {
        if (!L)
            Init();

        lastScriptPath = path;
        SaveLastScriptPath();

        GS::UniString fullPath;
        if (path.size() >= 2 && path[1] == ':') {
            fullPath = path.c_str();
        } else {
            IO::Location ownLoc;
            if (ACAPI_GetOwnLocation(&ownLoc) != NoError)
                return;
            ownLoc.DeleteLastLocalName();

            GS::UniString baseStr;
            ownLoc.ToPath(&baseStr);
            baseStr.Append("\\");
            baseStr.Append(path.c_str());
            fullPath = baseStr;
        }

        const char* cPath = fullPath.ToCStr().Get();

        // Load file as a function (allows setting debug hook before execution)
        if (luaL_loadfile(L, cPath) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            if (err) {
                ACAPI_WriteReport(err, true);
            } else {
                ACAPI_WriteReport("Script compile error", true);
            }
            lua_pop(L, 1);
            return;
        }

        // If a DAP client is attached, set the debug hook and request initial pause
        if (m_debugger.HasClient()) {
            lua_sethook(L, LuaDebugger::DebugHook, LUA_MASKLINE, 0);
            m_debugger.NotifyRunStarting();
        }

        // Execute the function on the stack
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            if (err) {
                ACAPI_WriteReport(err, true);
            } else {
                ACAPI_WriteReport("Script error (unknown)", true);
            }
            lua_pop(L, 1);
        } else {
            ACAPI_WriteReport("Script finished successfully (check Report window for output)", true);
        }

        // Clean up debug hook
        lua_sethook(L, nullptr, 0, 0);
        m_debugger.NotifyRunEnded();
    }

    const std::string& GetLastScriptPath() const { return lastScriptPath; }

    void Shutdown()
    {
        m_debugger.Stop();
        if (L) {
            lua_close(L);
            L = nullptr;
        }
    }

    lua_State* State() { return L; }

    // Exposed so LuaConsole callback can forward print() to DAP
    LuaDebugger m_debugger;

private:
    lua_State* L = nullptr;
    std::string lastScriptPath;

    static const wchar_t* RegKey() { return L"Software\\GRAPHISOFT\\ArchiLua"; }

    void LoadLastScriptPath()
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, RegKey(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t buf[1024] = {};
            DWORD size = sizeof(buf);
            DWORD type = 0;
            if (RegQueryValueExW(hKey, L"LastScriptPath", nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && type == REG_SZ) {
                int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    lastScriptPath.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, buf, -1, &lastScriptPath[0], len, nullptr, nullptr);
                }
            }
            RegCloseKey(hKey);
        }
    }

    void SaveLastScriptPath()
    {
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, RegKey(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, lastScriptPath.c_str(), -1, nullptr, 0);
            if (wlen > 0) {
                std::wstring wbuf(wlen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, lastScriptPath.c_str(), -1, &wbuf[0], wlen);
                RegSetValueExW(hKey, L"LastScriptPath", 0, REG_SZ, (const BYTE*)wbuf.c_str(), (DWORD)(wlen * sizeof(wchar_t)));
            }
            RegCloseKey(hKey);
        }
    }
};

} // namespace ArchiLua
