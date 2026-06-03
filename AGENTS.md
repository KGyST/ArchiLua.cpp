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
|---|---|---|
| `src/Bridge/LuaBridge.hpp` | `Bridge` class: wraps `lua_State*`, `Init`/`ExecuteScript`/`Shutdown` |
| `src/API/APIModule.hpp` | Lua API functions (`getsel`, `getwall`, `get`, `getpoly`, `getparams`) |
| `src/Console/LuaConsole.hpp` | Overrides Lua `print()` → `ACAPI_WriteReport` |
| `src/ArchiLua.cpp` | DLL entry, `CheckEnvironment`, `RegisterInterface`, `Initialize`, `MenuCommandHandler` |
| `deps/CMakeLists.txt` | Lua 5.4.7 FetchContent, generates `ArchiLuaDeps.props` |
| `RINT/ArchiLua.grc` | Localized STR# resources (name, description, menu) |
| `RFIX/ArchiLuaFix.grc` | Non-localized MDID resource (add-on ID: `0`, `4015391855`) |
| `local.props` | Per-machine paths (`ACBuildSupport`, `CommonLibs.cpp`) |
| `CommonLibs.cpp/` | Submodule: Logger, DateTime, WinReg, Utils, AC27.hpp |

All headers are explicitly listed in `ClInclude` in `ArchiLua.vcxproj` so they appear in Visual Studio's Solution Explorer. When adding a new header, add it to the `ClInclude` group too.

## Common pitfalls

- **Missing deps/build/ArchiLuaDeps.props** → run cmake above first.
- **Lua script not found** → ensure `lua_scripts/` dir exists next to the `.apx` file (or rebuild + re-register add-on).
- **`LLs` / `C2614` template errors** → sol2 was accidentally re-included; remove it.
- **`_CrtDbgReport` linker errors** → missing `ucrtd.lib` in debug link deps.
- **Logger linker errors** → missing `DateTime.cpp` or `WinReg.cpp` in `ClCompile` group.

## Pre-commit hook (clang-tidy)

A pre-commit hook runs `clang-tidy` on staged `.cpp`/`.hpp` files in `Src/`:
```bash
# One-time setup:
git config core.hooksPath githooks
```
The hook requires `compile_commands.json` at the project root (committed).  
Regenerate it with `generate-compile-commands.cmd` when source files change.

Clang 18 must be installed at `C:\Program Files\clang+llvm-18.1.8-x86_64-pc-windows-msvc`.  
The hook sets `VCToolsInstallDir` to VS 2022 v17 (MSVC 14.29) for STL compatibility.

## Commands

```bash
git submodule update --init          # clone CommonLibs.cpp
cmake -S deps -B deps/build          # re-generate deps
msbuild ... /t:Clean                 # clean build outputs
```

# Project Rules
- Always consult /adr/ before making significant architectural changes.
- If you implement any mock or stub, you MUST create a new ADR file in /docs/adr/ following the standard template.

### ARCHITECTURAL DECISION LOGGING PROTOCOL (ADR)

1. **Mandatory Documentation:** Significant technical decision must be documented in an ADR file.
2. **Storage Location:** Save all files in the project root: `/adr/`.
3. **Naming Convention:** Use `YYMMDDDD-short-description.md`. Example date format: 260601H, 260603Sze (H for Hétfő, Sze for Szerda: Hungarian weekdays) 
4. **Logging Policy:** MUST be logged as a separate ADR:
	- Any decision involving 'mocking' or 'stubbing' external dependencies
	- Any decision that involves asking User for an explict decision (If User provides an explanation, that must be summarized.) 
5. **Structure:**
   # ADR: [Title]
   - Status: [Accepted/Draft]
   - Date: [YYYY-MM-DD]
   - Context: [The technical challenge]
   - Decision: [The proposed solution]
		- Every offered/considered Alternative must be mentioned
   - Pro/Con Analysis: [Strictly objective, critical assessment]
		- Per Alternative
   - Exit Strategy: [Applicable for mocks only]
	 
	