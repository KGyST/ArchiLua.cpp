#pragma once

#include <sol/sol.hpp>
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
        if (initialized)
            return;

        lua.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::table,
            sol::lib::string,
            sol::lib::coroutine
        );

        RegisterConsole(lua);
        initialized = true;
    }

    void ExecuteScript(const std::string& path)
    {
        if (!initialized)
            Init();

        lua.script_file(path);
    }

    void Shutdown()
    {
        if (!initialized)
            return;

        initialized = false;
    }

    sol::state& State() { return lua; }

private:
    sol::state lua;
    bool initialized = false;
};

} // namespace ArchiLua
