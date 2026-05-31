#pragma once

#include <sol/sol.hpp>
#include "ACAPinc.h"

namespace ArchiLua {

inline void RegisterConsole(sol::state& lua)
{
    lua.set_function("print", [](const std::string& msg) {
        ACAPI_WriteReport(msg.c_str(), false);
    });
}

} // namespace ArchiLua
