#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <string>

#include "../Console/LuaConsole.hpp"
#include "../API/APIModule.hpp"

namespace ArchiLua {

class Bridge {
public:
    Bridge() = default;
    ~Bridge() { Shutdown(); }

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    void Init()
    {
        if (L)
            return;
        L = luaL_newstate();
        luaL_openlibs(L);

        LuaConsole::Register(L);
        APIModule::Register(L);
    }

    void ExecuteScript(const std::string& path)
    {
        if (!L)
            Init();

        lastScriptPath = path;

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
        if (luaL_dofile(L, cPath) != LUA_OK) {
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
    }

    const std::string& GetLastScriptPath() const { return lastScriptPath; }

    void Shutdown()
    {
        if (L) {
            lua_close(L);
            L = nullptr;
        }
    }

    lua_State* State() { return L; }

private:
    lua_State* L = nullptr;
    std::string lastScriptPath;
};

} // namespace ArchiLua
