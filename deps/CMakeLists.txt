cmake_minimum_required(VERSION 3.20)
project(ArchiLuaDeps LANGUAGES C CXX)

cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0169 OLD)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# -------------------------------------------------------------------
# Lua 5.4.7 - Fetch source, build as static library
# -------------------------------------------------------------------
FetchContent_Declare(
  lua
  URL      https://www.lua.org/ftp/lua-5.4.7.tar.gz
  URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
)

FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
  message(STATUS "Fetching Lua 5.4.7...")
  FetchContent_Populate(lua)
endif()

file(GLOB lua_src "${lua_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM lua_src "${lua_SOURCE_DIR}/src/lua.c" "${lua_SOURCE_DIR}/src/luac.c")

add_library(lua54 STATIC ${lua_src})
target_include_directories(lua54 PUBLIC "${lua_SOURCE_DIR}/src")
set_target_properties(lua54 PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
)

# -------------------------------------------------------------------
# sol2 3.3.0 - Header-only library
# -------------------------------------------------------------------
FetchContent_Declare(
  sol2
  URL https://github.com/ThePhD/sol2/archive/refs/tags/v3.3.0.tar.gz
)

FetchContent_GetProperties(sol2)
if(NOT sol2_POPULATED)
  message(STATUS "Fetching sol2 3.3.0...")
  FetchContent_Populate(sol2)
endif()

# -------------------------------------------------------------------
# Generate MSBuild property sheet for ArchiLua.vcxproj
# -------------------------------------------------------------------
set(LUA_INCLUDE_DIR "${lua_SOURCE_DIR}/src")
set(LUA_LIB_DIR     "${CMAKE_BINARY_DIR}")
set(LUA_LIB_NAME    "lua54")
set(SOL2_INCLUDE_DIR "${sol2_SOURCE_DIR}/include")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/ArchiLuaDeps.props.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ArchiLuaDeps.props"
  @ONLY
)

message(STATUS "")
message(STATUS "============================================")
message(STATUS "  ArchiLua Dependencies Generated")
message(STATUS "  Lua include : ${LUA_INCLUDE_DIR}")
message(STATUS "  Lua lib     : ${LUA_LIB_DIR}/${LUA_LIB_NAME}.lib")
message(STATUS "  sol2 include: ${SOL2_INCLUDE_DIR}")
message(STATUS "  Props file  : ${CMAKE_CURRENT_BINARY_DIR}/ArchiLuaDeps.props")
message(STATUS "============================================")
message(STATUS "")

# Build the props file output path so it's always valid
file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ArchiLuaDeps.props" DEPS_PROPS_PATH)
message(STATUS "Import this in your vcxproj:")
message(STATUS "  <Import Project=\"${DEPS_PROPS_PATH}\" Condition=\"Exists('${DEPS_PROPS_PATH}')\" />")
