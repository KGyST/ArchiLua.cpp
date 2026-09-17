# ROADMAP.md

## Phase 1: Foundation (The Bridge) ✓
  - AC27-only add-on skeleton (PolygonReducer template, new GUIDs); Lua 5.4.7 via CMake FetchContent with raw C API (sol2 dropped over `/Zc:wchar_t-`); `print()` → `ACAPI_WriteReport`; `try_hello.lua` menu command.

## Phase 1.5: Minimalist GUI ✓
  - Modal dialog with file picker (`DG::FileDialog`) + Run button; later superseded by the modeless palette.

## Phase 2: Data Pipeline (Reading) ✓
  - `acapi.getSel()` selection GUIDs; wall memo → Lua tables (coords, openings via `BMGetPtrSize`).

## Phase 2.5: Extended Reading and Fixes ✓
  - Generic `acapi.get/getPoly/getWall/getparams`, `try_selection.lua` demo; DAP debugger attach; modal → modeless (`DG::Palette`) GUI; last-script-path registry persistence; clang-tidy pre-commit hook; type-name mapping ADR (C++ switch).
	
## Phase 3: The Action (Writing) ✓
  - Undoable writes (`beginundo`/`endundo` + auto undo steps); `setwall`/`set`/`setparams` via masked `ACAPI_Element_Change`; `findobject` library search; `create` element placement with GDL overrides.

## Phase 3.5: Element Creation ✓
  - `addWall` (straight + polygonal), `addWindow`/`addDoor` with side/mirror control, `addSlab`, `addRoof`; `try_add_wall.lua` demo.

## Phase 3.6: Event Dispatcher Table (EDT) — Lua-Driven Web GUI ✓
- [x] **Web Palette Skeleton:** Modeless `DG::Palette` with `DG::Browser` (WebView2).
  - HTML/JS embedded in C++ (`BuildHTML()`), JS bridge via `RegisterAsynchJSObject`.
  - `archilua.DispatchEvent(eventName)` calls C++ which routes to Lua callbacks.
- [x] **EDT Core — JS → Lua Dispatch:**
  - `RegisterWebEvent(name, callback)` exposed to Lua — stores callbacks in a registry table.
  - `SetWebResult(text)` — calls `browser.ExecuteJS()` to update the HTML result div.
  - `PickWall()` — Lua-callable wrapper around `ClickAnElem(API_WallID)`.
  - `ShowWebDialog()` — creates and shows the `LuaWebDialog` palette.
  - `DispatchUIEvent()` looks up the event name in the Lua callback table, calls it via `lua_pcall`.
- [x] **Menu Restructure:**
  - **Menu 1** (dev tool): File picker to load + run any `.lua` script. The script can call `ShowWebDialog()` to display a web UI.
  - **Menu 2** (shortcut): Loads `try_web_gui.lua` directly (shorthand for the common path).
  - **Long term:** Menu 1 stays as dev/scripting tool; Menu 2 becomes a plugin loader for `.lua` plugins.
- [x] **Single-File Script Pattern:** A `.lua` script calls `RegisterWebEvent()` for each UI event, then `ShowWebDialog()` — the script IS the plugin, self-contained.
- [x] **IPC Chain (verified working):**
  JS `archilua.DispatchEvent('onPickWall')` →
  C++ `DispatchUIEvent` → Lua callback →
  C++ `ClickAnElem` → result to Lua →
  `SetWebResult()` → `ExecuteJS` → HTML DOM updated.
- [x] **Window Placing Test Script:** Place a window into the picked wall via `acapi.addWindow` from the Lua callback (`lua_scripts/try_window_placer.lua`).
  - Added UI controls (numeric inputs, dropdowns, angles) and corresponding EDT events (`onPlaceWindow`).
  - GUI has a button to add a window with parameters (position, height, width, sill, refSide, oSide, mirrored, opening angle).
- [x] **Window Modifier:** The Window follows UI changes back and forth.
  - Windows on the picked wall are listed in a dropdown selection.
  - Selecting a window loads its properties (`objLoc`, width, height, sill, refSide, oSide, mirrored, opening angle) into the UI entries via `acapi.getWindow()`.
  - Changing UI entries and clicking "Update Window" updates the window via `acapi.setWindow()`.
- [x] **Windows Registry Handling:**
  - `acapi.regRead(section, key[, default])` / `acapi.regWrite(section, key, value)` in `src/API/APIModule.hpp`, strictly scoped to `HKCU\Software\Samu\ArchiLua\<section>\` (section/key validated to `[A-Za-z0-9_]{1,64}`; values stored as `REG_SZ`).
  - Self-contained WinAPI implementation (two-step sized reads, every `LSTATUS` checked) instead of `CommonLibs` WinReg helpers: `GetRegString()` silently drops values > 255 chars and `GetOrCreateRegPath()` uses the `HKEY` unchecked.
  - `lua_scripts/try_window_placer.lua` persists all form fields on save and restores them on load (section `try_window_placer`).
	
## Phase 3.7: Minimalist Observer (PoC Only)
- [x] **Minimal C++ API Binding:**
  - Expose two simple C++ functions to Lua:
    - `acapi.watch(guid, func_url, kwargs)` → calls `ACAPI_Element_AttachObserver(guid)`.
      - `func_url` is `"script.lua\\FunctionName"` (script part recorded for future multi-script support; currently the function name is resolved as a Lua global in the shared state).
      - `kwargs` (Lua table) is held via registry ref for the session AND serialized to JSON into `ACAPI` Project Storage (ModulData): `{guid: {func_url, kwargs}}`, so watches survive project reload. Prune GUIDs that no longer exist on load.
			E.g. `{"46358EF2-DF29-4BA2-B313-51A8F064CA02": {"try_observer.lua\\onWallEvent", {"note": "test"}}}`
    - `acapi.unwatch(guid)` → calls `ACAPI_Element_DetachObserver(guid)`. Clears the `guid` entry from the session map and Project Storage (also drops any pending Edit).
  - In the global `APIElementEventHandlerProc` callback:
    - On `APINotifyElement_Edit`, do NOT call Lua immediately — store/overwrite a pending entry per guid.
    - On `APINotifyElement_Change`, first dispatch the pending Edit (if any) as `func(guid, kwargs, "edit")`, then dispatch `func(guid, kwargs, "change")`. Lua is notified of both kinds but runs only for the last Edit in a row (drag end) plus the drop.
    - Ignore property changes, classifications, transform matrices, and undo/redo variants (DB writes are forbidden during Undo/Redo).
    - Reentrancy guard: notifications triggered by our own callback's DB writes are skipped (delete-and-re-add on the watched wall would otherwise recurse).
	- A new standalone `try_observer.lua` tests this functionality (independent from the window placer script).

- [x] **Lua Integration (`try_observer.lua`):**
  - Upon picking a wall (triggered by the UI pick button), call `acapi.watch(wallGuid, "try_observer.lua\\onWallEvent", {note = "..."})`.
  - Define the callback function:
    - `function onWallEvent(guid, kwargs, kind)`: log `kind` (`"edit"`/`"change"`) + counter via `SetWebResult`.
  - Verified: one coalesced `edit` + one `change` per drag; re-watch of a watched wall re-arms cleanly; `observerLog` tracer toggle.

- [ ] **Verification Gate:**
  - Drag or move the watched wall in ArchiCAD 2D/3D viewport.
  - Verified so far: one coalesced `edit` arrives at drop (no flood during drag). Reload-persistence restore not yet verified.
  - Do NOT implement watched-lists, UI observer panels, or complex C++ classes.

- [ ] **Divider Demo (`try_dividers.lua`, user-data row model):**
  - Row (`dividers`, `libInd`, `count`, `opts`) lives ON THE WALL via generic `acapi.setUserData/getUserData/deleteUserData` (JSON handle, any element type, undoable) — source of truth, travels with the element through save/load/undo. Registry keeps UI prefs only. Behavior (not storage) is the delete function: every sync path (tick/manual/event, rotation rebuild, re-place) deletes listed panels and re-runs division fresh; `DIVCOUNT` configurable (setting vs live-row split).
  - Place 10 standalone divider panels (library objects on the wall centerline) on a picked wall, watch it, recompute XY from `begC`/`endC` and move them via `acapi.set(pos)` on wall `edit`/`change`. Panel part name is user-configurable (persisted in registry); panels removable via `acapi.delete`.
  - Verified end-to-end with a placeable part (`'ágy 01 27'`): row tiles the wall, params apply via post-create `Change`, drag/stretch sync follows.
  - Brick-laying params on place: `A` = division spacing (tiles the wall), `B` = wall width (auto, or fixed UI entry), `ZZYZX` = 0.25 (UI entry); all three re-applied via `setParams` on sync. Panel rotation follows the wall vector (`angle` on create); wall direction change deletes + recreates in 2 undo steps (`angle` is not `Change`-editable per DevKit).
  - UI: center-on-point shift checkbox, auto-refresh on field edit (debounced), log-events checkbox, Inspect Part (params + lib flags), event log panel, per-item error messages. Session state (wall, dividers, opts, watch flag) survives script re-runs via registry.
  - `findObject` verifies the match (case-insensitive) and rejects template/non-placeable parts loudly (e.g. `m_Viapanel_Wallpanel` fails `Create` with `BADPARS` — presumed non-placeable, flags confirmation pending); `create`/`createMany` report override counts and warn on zero matches.
  - API names are case-sensitive: script/DOC/register audit done, single mismatch (`setparams` → `setParams`) fixed.
  - Undo: `createMany` places the row in one step; `begin/endUndo` batch `set`/`setParams`/`delete` (mask-merged, deletes-first replay).
  - [x] **Undo scope (converge + guide, not unify):** a single shared undo unit is impossible (the API cannot join ArchiCAD's open drag operation — nested undoable commands are refused mid-drag with `APIERR_REFUSEDCMD`). Accepted design: wall and row undo separately (division first, wall second); all 10 panels + the wall are watched, `Undo/Redo_*` forward as `undo`/`redo` kinds (dispatch only, zero writes), heartbeat re-syncs within ~1 s. A reverted sync (panels back, wall unchanged) is GUIDED, never rewritten — rewriting it would trap wall-undo behind auto-redo. Tick compares each panel pos via `acapi.getPos` against expected and explains the next Ctrl+Z (10 s latch).
  - [ ] **Bugfix: single-step rotation re-sync.** Implemented as `acapi.syncRow` (moves + deletes + creates in one undoable command); awaiting user verification.
  - [ ] **Replace JS heartbeat with `CallFromEventLoop` deferral (deadline: next C++ batch).** The 800 ms poll works but lags post-drop and smells; the DevKit primitive posts the sync into the main event loop from the notification handler — event-driven, no polling.
  - Needs: `acapi.set` `pos` support for objects + `acapi.delete` (done); `acapi.create` lib-type→element-type mapping fixed — `libPart.typeID` is `API_LibTypeID`, never cast to `API_ElemTypeID` (done); `ShowWebDialog` replaces the previous script's GUI instead of showing stale content (done).

- [ ] **Observer Polish / Finalize (deferred):**
  - Decide `observerLog` fate (keep as diagnostic or remove); verify reload-persistence; revisit Edit-during-drag semantics if ArchiCAD behavior differs per operation.

## Phase 3.8: PolygonReducer Port to ArchiLua (Interactive PoC)
- [ ] **Reference Code Analysis:**
  - Read reference implementation from `docs/reference/PolygonReducer.cpp` (or local repo reference).
  - Identify required C++ Bridge extensions for `API_ElementMemo` handling:
    - Ensure `acapi.getpoly(guid)` exports both vertex coordinates AND arc segments (`API_PolyArc` array) to Lua.
- [ ] **C++ Metadata Bridge:**
  - Ensure Lua can read/write element Property/ID metadata to store parent-child relationships (e.g. `acapi.setproperty(guid, key, value)` or storing `parentGuid`).
- [ ] **Interactive Test Script (`lua_scripts/try_polygon_reducer.lua`):**
  - **Single-File GUI:** Open a Modeless `DG::WebView` containing:
    - Slider: target point count (3 to N).
    - Entry: minimum edge length threshold.
    - Button: "Pick Polyline/Slab".
  - **Lua Preprocessing & Reduction Logic:**
    - Implement arc vectorization (converting arcs into segmented vertices based on edge length).
    - Implement collinear midpoint removal (filtering out redundant vertices on straight edges).
    - Implement the core reduction algorithm in Lua based on the GUI controls.
  - **Live Redraw & Metadata Tracking:**
    - GUI callbacks trigger `acapi.drawfeedback(reducedPoly)` on slider/entry input for live preview.
    - On confirmation/apply, create the new reduced element and attach metadata referencing the original parent GUID.
		
## Phase 3.9: Async Minizinc Discrete Optimization


## Phase 4: Stability & Logic (The MVP)
- [ ] **GC Safety:** C++ side `collectgarbage("stop")` before ACAPI calls and `collectgarbage("collect")` on scope exit.
- [ ] **Transaction Exception Safety:** Ensure that if a Lua script throws an error between `beginundo` and `endundo`, the C++ host gracefully aborts/closes the open ACAPI transaction to prevent DB corruption.
- [ ] **Transformation Logic:** Emulate GDL-style `ADD`, `MUL`, `ROTX` stack within Lua for panel alignment.

---
### v0.1.0 - MVP REACHED

## Phase 5: Scaling (Post-MVP)
- [ ] **Userdata Migration:** Replace Lua tables with full userdata + metatables for element objects. Adds `__index` for lazy access, `__newindex` for writes, `__gc` for cleanup, type identity via metatable comparison.
- [ ] **GUI Integration:** `LUA-LIMGUI` (Dear ImGui) overlay for real-time parameter tweaking.
- [x] **Event Listeners:** Lua callbacks triggered by ArchiCAD element modification events. **MOVED to Phase 3.7** (own phase, was listed here). A wall modified can trigger a lua script again that was run on that wall.
- [ ] **Automated Header Export:** Python/Clang-AST script to batch-generate Lua bindings for the full AC API.
- [ ] **ArchiCAD 28/29:** support
- [ ] **SamuTeszt Hook:** JSON dump of Lua tables before/after placement for regression testing.

## Phase 6: Generalizing (Support other Languages) 
- [ ] **Python Interpreter Integration:** A Python (and later other languages) interpreter to be integrated
  - A common API interface / Bridge is to be defined, so that multiple language interpreters can be added later on

