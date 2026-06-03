#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "ACAPinc.h"

namespace ArchiLua {
namespace LuaConsole {

static void (*g_outputCB)(const char*) = nullptr;

static int Print(lua_State* L)
{
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        if (s) {
            ACAPI_WriteReport(s, false);
            if (g_outputCB)
                g_outputCB(s);
        }
    }
    return 0;
}

inline void Register(lua_State* L, void (*outputCB)(const char*) = nullptr)
{
    g_outputCB = outputCB;
    lua_register(L, "print", Print);
}

} // namespace LuaConsole
} // namespace ArchiLua
