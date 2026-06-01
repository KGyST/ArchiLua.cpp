# ArchiLua.cpp — Agent Guide

## Project

ArchiCAD 27 add-on (.apx) embedding Lua 5.4 via raw C API.  
C++17, x64 only, VS 2022 v142 toolset.

## Build (two-stage)

```bash
# 1. Fetch + build Lua 5.4.7, generate ArchiLuaDeps.props
cd deps
cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Release

# 2. Build add-on
msbuild ArchiLua.sln /p:Configuration="Debug 27" /p:Platform=x64
```

## Critical constraints

- **`/Zc:wchar_t-`** is required by ArchiCAD headers. `wchar_t` is `unsigned short`.
- **Do NOT add sol2** — incompatible with `/Zc:wchar_t-`. Use raw Lua C API (`lua_State*`, `luaL_dofile`).
- **GRC resource pipeline**: three CustomBuild steps in vcxproj. GRC → ResConv → .rc2 → rc.exe → .res. The two `CustomBuild` entries for `RINT/` and `RFIX/` have swapped commands in the vcxproj — do not reorder them.
- **`lua_scripts/`** resolved at runtime via `ACAPI_GetOwnLocation` + `DeleteLastLocalName`, relative to add-on `.apx` path.
- **DllMainEntry** (not DllMain), **FastCall** calling convention.
- **Debug CRT** needs `ucrtd.lib;msvcrtd.lib;msvcprtd.lib` — not just `msvcrtd.lib`.
- **`/FS` compiler flag** needed in Debug + Release to avoid C1041 PDB contention.
- **`FTM::FileTypeManager`** requires a unique ID string (`"ArchiLua"`) — default constructor is private (singleton pattern).

## Structure

| Path | Purpose |
|---|---|
| `src/Bridge/LuaBridge.hpp` | `Bridge` class: wraps `lua_State*`, `Init`/`ExecuteScript`/`Shutdown` |
| `src/Console/LuaConsole.hpp` | Overrides Lua `print()` → `ACAPI_WriteReport` |
| `src/ArchiLua.cpp` | DLL entry, `CheckEnvironment`, `RegisterInterface`, `Initialize`, `MenuCommandHandler` |
| `deps/CMakeLists.txt` | Lua 5.4.7 FetchContent, generates `ArchiLuaDeps.props` |
| `RINT/ArchiLua.grc` | Localized STR# resources (name, description, menu) |
| `RFIX/ArchiLuaFix.grc` | Non-localized MDID resource (add-on ID: `0`, `4015391855`) |
| `local.props` | Per-machine paths (`ACBuildSupport`, `CommonLibs.cpp`) |
| `CommonLibs.cpp/` | Submodule: Logger, DateTime, WinReg, Utils, AC27.hpp |

## Common pitfalls

- **Missing deps/build/ArchiLuaDeps.props** → run cmake above first.
- **Lua script not found** → ensure `lua_scripts/` dir exists next to the `.apx` file (or rebuild + re-register add-on).
- **`LLs` / `C2614` template errors** → sol2 was accidentally re-included; remove it.
- **`_CrtDbgReport` linker errors** → missing `ucrtd.lib` in debug link deps.
- **Logger linker errors** → missing `DateTime.cpp` or `WinReg.cpp` in `ClCompile` group.

## Commands

```bash
git submodule update --init          # clone CommonLibs.cpp
cmake -S deps -B deps/build          # re-generate deps
msbuild ... /t:Clean                 # clean build outputs
```
