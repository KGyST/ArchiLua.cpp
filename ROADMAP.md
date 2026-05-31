# ROADMAP.md

## Phase 1: Foundation (The Bridge)
- [ ] **Plugin Skeleton:** ArchiCAD Add-on project setup with custom dev-template.
  - Create the folder structure and project files
  - Use E:\Git\PolygonReducer.cpp project files and folder structure as a skeleton
	- Assign new guids, remove unnecessary included headers and object files
- [ ] **Lua Integration:** Linking Lua 5.4 static lib and `sol2` header-only wrapper via CMake.
- [ ] **Internal Console:** `ACAPI_WriteReport` wrapper to redirect Lua `print()` to ArchiCAD's session report.
- [ ] **The "Hello ArchiLua":** Command-switch to execute a hardcoded `try_hello.lua` from the `/scripts` folder.

## Phase 2: The Data Pipeline (Reading)
- [ ] **Selection Proxy:** Wrap `ACAPI_Selection_Get` to return a list of GUID strings to Lua.
- [ ] **Wall Geometry (The Memo):** - Convert `API_Element` (ID, Layer) to Lua table.
    - Convert `API_ElementMemo.coords` (Handle) to indexed Lua table `{ {x1, y1}, {x2, y2} ... }`.
- [ ] **Opening Extraction:** Extract doors/windows from `API_ElementMemo` to detect layout voids.

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

