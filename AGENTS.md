# ArchiLua.cpp — Agent Guide

## Specification Files
- **ARCHITECTURE.md** Project technical description and project structure
- **README.md** Project user level description and docs
- **ROADMAP.md** Development pipeline
  - If a phase is ready, compact the previous one: summarize its content in a short description.
- Always consult `Architectural Decision Records/` before making significant architectural changes.
- If you implement any mock or stub, you MUST create a new ADR file in `Architectural Decision Records/` following the standard template.

## Build (two-stage)
```bash
# 1. Fetch + build Lua 5.4.7, generate ArchiLuaDeps.props
cd deps
cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Release

# 2. Build add-on (always clean first to force GRC resource recompilation)
msbuild ArchiLua.sln /p:Configuration="Debug 27" /p:Platform=x64 "/t:Clean;Build"
```
**Always build `Debug 27`.**
`assert()` is the runtime check mechanism — it's active in Debug CRT and the reason we use the Debug configuration.
Release builds are used only for final packaging; never for development iteration.

If ArchiCAD is running and locks the `.pdb` or `.apx` (LNK1104: cannot open file), report to the user and ask them to close ArchiCAD.
Do NOT kill ArchiCAD automatically.

## Critical constraints

- **`/Zc:wchar_t-`** is required by ArchiCAD headers. `wchar_t` is `unsigned short`.
- **Do NOT add sol2** — incompatible with `/Zc:wchar_t-`. Use raw Lua C API (`lua_State*`, `luaL_dofile`).
- **GRC resource pipeline**: three CustomBuild steps in vcxproj. GRC → ResConv → .rc2 → rc.exe → .res. The two `CustomBuild` entries for `RINT/` and `RFIX/` have swapped commands in the vcxproj — do not reorder them.
- **`lua_scripts/`** resolved at runtime via `ACAPI_GetOwnLocation` + `DeleteLastLocalName`, relative to add-on `.apx` path.
- **DllMainEntry** (not DllMain), **FastCall** calling convention.
- **Debug CRT** needs `ucrtd.lib;msvcrtd.lib;msvcprtd.lib` — not just `msvcrtd.lib`.
- **`/FS` compiler flag** needed in Debug + Release to avoid C1041 PDB contention.
- **`FTM::FileTypeManager`** requires a unique ID string (`"ArchiLua"`) — default constructor is private (singleton pattern).

## Common pitfalls

- **Missing deps/build/ArchiLuaDeps.props** → run cmake above first.
- **Lua script not found** → ensure `lua_scripts/` dir exists next to the `.apx` file (or rebuild + re-register add-on).
- **`LLs` / `C2614` template errors** → sol2 was accidentally re-included; remove it.
- **`_CrtDbgReport` linker errors** → missing `ucrtd.lib` in debug link deps.
- **Logger linker errors** → missing `DateTime.cpp` or `WinReg.cpp` in `ClCompile` group.
- **DAP/ws2_32.lib linker errors** → `ws2_32.lib` must be linked in both Debug and Release (already in vcxproj).
- **DAP not connecting** → check firewall or ensure no other process uses port 4711. Start ArchiCAD before attaching VS Code debugger.
- **`DG::Palette` modeless dialog** — `DG::Palette` does NOT have `Close()` or `PostCloseRequest()` (those are `DG::ModalDialog`-only). Use `SendCloseRequest()` (inherited from `ModelessBase`) to programmatically close. `PanelClosed` handler must `delete this` for self-deleting lifecycle. `DG::Palette` inherits from `ModelessBase`, not `DG::Panel`.

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

# Commit conventions
- If a `[x]` marked feature is finished, a commit must be done
- During a longer/harder development, smaller verified by user improvements must be commited by a temporary commit (commit message starting `_` ).
- **EoD / EoW / EoY** — End of Day, End of Week, End of Year temp commits (like `_temp`). EoD messages must start with `EoD` etc. Must be done if user asks for them.
  - These placeholder commits must be amended when a `[x]` commit is done (a feature is finished and commited)
	
## Format conventions
- Date format is Hungarian `YYMMDDWw`, like `260808Szo` (for `Szombat`/`Saturday`) 
- `ROADMAP.md` Format:
  - `## Phase` for phases
    - `[ ]` / `[x]` for individual testable features
      - `-` for feature descriptions
	- After a Phase is done, the `-` all descriptions can be removed from the finished phase for compacting
	  - For all Phases before the just finished phases `[ ]` / `[x]` points can be removed and a summarization of them should be added. A finished Phase should look like this:
		`## Phase x: Short description
		  - Summary of features done` 

### ARCHITECTURAL DECISION LOGGING PROTOCOL (ADR)

1. **Mandatory Documentation:** Significant technical decision must be documented in an ADR file.
2. **Storage Location:** Save all files in the project root: `Architectural Decision Records/`.
3. **Naming Convention:** Use `YYMMDDDD-short-description.md`. Example date format: 260601H, 260603Sze (H for Hétfő, Sze for Szerda: Hungarian weekdays) 
4. **Logging Policy:** MUST be logged as a separate ADR:
	- Any decision involving 'mocking' or 'stubbing' external dependencies
	- Any decision that involves asking User for an explict decision (If User provides an explanation, that must be summarized.) 
5. **Structure (use semantic line breaks — one sentence per line for readability):**
   # ADR: [Title]
   - Status: [Accepted/Draft]
   - Date: [YYMMDD]
   - Context: [The technical challenge]
   - Decision: [The proposed solution]
		- Every offered/considered Alternative must be mentioned
   - Pro/Con Analysis: [Strictly objective, critical assessment]
		- Per Alternative
   - Exit Strategy: [Applicable for mocks only]
	 
	