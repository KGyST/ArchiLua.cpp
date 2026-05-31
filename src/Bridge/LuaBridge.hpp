#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <string>

#include "../Console/LuaConsole.hpp"

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
    }

    void ExecuteScript(const std::string& relativePath)
    {
        if (!L)
            Init();

        IO::Location ownLoc;
        if (ACAPI_GetOwnLocation(&ownLoc) != NoError)
            return;
        ownLoc.DeleteLastLocalName();

        GS::UniString baseStr;
        ownLoc.ToPath(&baseStr);
        baseStr.Append("\\");
        baseStr.Append(relativePath.c_str());

        if (luaL_dofile(L, baseStr.ToCStr().Get()) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            if (err) {
                ACAPI_WriteReport(err, false);
            }
            lua_pop(L, 1);
        }
    }

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
};

} // namespace ArchiLua
