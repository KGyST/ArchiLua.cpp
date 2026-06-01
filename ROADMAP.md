# ROADMAP.md

## Phase 1: Foundation (The Bridge) ✓
- [x] **Plugin Skeleton:** ArchiCAD Add-on project setup via PolygonReducer custom template.
  - New GUIDs, AC27-only, stripped Boost/Geometry/GUI groups
- [x] **Lua Integration:** Lua 5.4.7 static lib via CMake FetchContent, raw C API (`lua_State*`, `luaL_dofile`).
  - **sol2 dropped** — incompatible with ArchiCAD's `/Zc:wchar_t-` requirement
- [x] **Internal Console:** `LuaConsole::Register` wraps `ACAPI_WriteReport` for Lua `print()`.
- [x] **"Hello ArchiLua":** Menu command executes `lua_scripts/try_hello.lua` relative to add-on location.

## Phase 1.5: Adding a minimalist GUI ✓
- [x] A modal dialog opens when menu item is clicked:
  - File path text edit + "..." browse button (uses `DG::FileDialog`)
  - "Run" button executes the selected Lua script

## Phase 2: The Data Pipeline (Reading) ✓
- [x] **Selection Proxy:** Wrap `ACAPI_Selection_Get` to return a list of GUID strings to Lua.
- [x] **Wall Geometry (The Memo):** Convert `API_Element` (ID, Layer) to Lua table.
    - Convert `API_ElementMemo.coords` (Handle) to indexed Lua table `{ {x1, y1}, {x2, y2} ... }`.
- [x] **Opening Extraction:** Extract doors/windows from `API_ElementMemo` via `BMGetPtrSize` (not null-sentinel).

## Phase 2.5: Extending Reading Functionality and Fixes
- [x] **Add more Object Types** to the reading functionality
  - Generic `acapi.get(guid)` returns guid, layer, typeName + polygon coords for walls/slabs
  - `acapi.getpoly(guid)` returns raw polygon vertex array for any element with a polygon
  - `acapi.getwall(guid)` kept as-is for detailed wall data (openings etc.)
- [x] **Write new Lua Scripts** to demonstrate reading of multiple object types
  - `lua_scripts/explore_selection.lua` — iterates selection, prints type + polygon for each
- [x] Convenience fixes
  - `ArchiLua` name → `_ArchiLua` prefix when debugging (appears first in add‑ons list)
  - `APIAddon_Normal` lifecycle already done — add-on loads on menu click, unloads after dialog closes
- [x] **Write Defaults to Registry** (persist last script path via Win32 registry helpers in Bridge)
- [ ] **Remote Debugging Bridge:** Integrate `mobdebug` or `LuaPanda` support for remote IDE attachment.
    - Implement a "Wait for Debugger" flag in the C++ host to allow breakpoint synchronization before script execution.
		
## Phase 3: The Action (Writing)
- [ ] **Object Finder:** Search Library Part by name (`m_Viapanel_Wallpanel`) and return its `LibIndex`.
- [ ] **Parameter Marshaling:** Map Lua tables `{ name = value }` to `API_AddParID` handles.
- [ ] **Placement:** Wrap `ACAPI_Element_Create` to instantiate objects at calculated coordinates.
- [ ] **Transaction Guard:** Implementation of `ACAPI_CallUndoableCommand` to ensure Lua-batch operations are a single "Undo" step.

## Phase 4: Stability & Logic (The MVP)
- [ ] **GC Safety:** C++ side `collectgarbage("stop")` before ACAPI calls and `collectgarbage("collect")` on scope exit.
- [ ] **SamuTeszt Hook:** JSON dump of Lua tables before/after placement for regression testing.
- [ ] **Transformation Logic:** Emulate GDL-style `ADD`, `MUL`, `ROTX` stack within Lua for panel alignment.

---
### v0.1.0 - MVP REACHED

## Phase 5: Scaling (Post-MVP)
- [ ] **GUI Integration:** `LUA-LIMGUI` (Dear ImGui) overlay for real-time parameter tweaking.
- [ ] **Event Listeners:** Lua callbacks triggered by ArchiCAD element modification events.
- [ ] **Automated Header Export:** Python/Clang-AST script to batch-generate `sol2` bindings for the full AC API.

