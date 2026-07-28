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

## Phase 2.5: Extending Reading Functionality and Fixes ✓
- [x] **Add more Object Types** to the reading functionality
  - Generic `acapi.get(guid)` returns guid, layer, typeName + polygon coords for walls/slabs
  - `acapi.getpoly(guid)` returns raw polygon vertex array for any element with a polygon
  - `acapi.getwall(guid)` kept as-is for detailed wall data (openings etc.)
- [x] **Write new Lua Scripts** to demonstrate reading of multiple object types
  - `lua_scripts/try_selection.lua` — iterates selection, prints type + polygon for each
- [x] Convenience fixes
  - `ArchiLua` name → `_ArchiLua` prefix when debugging (appears first in add‑ons list)
  - `APIAddon_Normal` lifecycle already done — add-on loads on menu click, unloads after dialog closes
  - Pre-commit hook (`githooks/pre-commit`) runs clang-tidy on staged `Src/` files
  - `generate-compile-commands.cmd` to regenerate the compilation database
- [x] **Write Defaults to Registry** (persist last script path via Win32 registry helpers in Bridge)
- [x] **ADR: Type-Name Mapping Strategy** — document decision to keep C++ switch (compile-time safe) for type→name mapping; add `acapi.getparams()` for generic GDL parameter access
- [x] **Generic Parameter Access** — `acapi.getparams(guid)` reads all GDL parameters for any library-part-based element (objects, doors, windows, columns, beams, zones)
- [x] **Debug Adapter Protocol:** Integrate Debug Adapter Protocol for remote IDE attachment.
- [x] **Modeless GUI:** Change now-modal GUI to modeless (`DG::Palette`). When "Run" button is pressed, the script runs in blocker mode (sync). 
		
## Phase 3: The Action (Writing) ✓
- [x] **Transaction Guard:** `acapi.beginundo(label)` / `acapi.endundo()` — buffers changes in `PendingChange` vector, flushes in a single `ACAPI_CallUndoableCommand`. Standalone writes (outside begin/end) auto-create their own undo step via `ACAPI_CallUndoableCommand`.
- [x] **Object Modification:** `acapi.setwall(guid, table)` modifies wall properties (height, thickness, layer, begC/endC) and writes back via `ACAPI_Element_Change` with mask.
- [x] **Generic Setter:** `acapi.set(guid, table)` for any element type — reads `API_Element_Get`, applies `layer` (common field), dispatches type-specific fields.
- [x] **Parameter Writing:** `acapi.setparams(guid, {name=value})` writes GDL parameters via `ACAPI_Element_GetMemo(APIMemoMask_AddPars)` + `ACAPI_Element_Change(APIMemoMask_AddPars)` pattern.
- [x] **Object Finder:** `acapi.findobject(name)` searches Library Part by name via `ACAPI_LibraryPart_Search`, returns `libInd` + display name.
- [x] **Placement:** `acapi.create(libInd, position, params)` wraps `ACAPI_Element_Create` with optional initial GDL parameter overrides.

## Phase 3.5: Element Creation ✓
- [x] **Create Wall:** `acapi.addWall({begC={x,y}, endC={x,y}, height, thickness, layer})` — creates a new wall element via `ACAPI_Element_Create` wrapped in an undoable command.
- [x] **Create Window in Wall:** `acapi.addWindow(wallGuid, {objLoc, height, width, sillHeight, wallSide, mirrored})` — places a window in a straight wall with side/mirror control.
- [x] **Example Script:** `lua_scripts/try_add_wall.lua` demonstrates wall + window creation.
- [x] **Polygonal Walls:** `addWall` with `poly` table for polygonal wall geometry via `API_ElementMemo.coords`.
- [x] **Wall Side + Mirroring:** `addWindow` with `wallSide` ("inside"/"outside") and `mirrored` params. `addDoor` analogous.
- [x] **Doors:** `acapi.addDoor(wallGuid, params)` — places a door in a wall (mirrors `addWindow` with `API_DoorID`).
- [x] **Slabs:** `acapi.addSlab({poly, thickness, layer, floor})` — creates a polygonal slab element.
- [x] **Roofs:** `acapi.addRoof({poly, thickness, layer, floor})` — creates a polygonal roof element.

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
- [ ] **Window Placing Test Script:** Place a window into the picked wall via `acapi.addWindow` from the Lua callback.
  - Add more UI controls (numeric inputs, dropdowns, slider) and corresponding EDT events.
	- The GUI should have a Button to add a windows, entries for Window X and Y (Sill) positions and Width and Height. When Button is pressed, a new Window is added having given parameters.
- [ ] **Windows Registry Handling:**
  - Persist GUI input field values into Windows Registry under `HKCU\Software\Samu\ArchiLua\try_web_gui`.
  - Create Win32 Registry C++ helper wrappers based on `CommonCppLibs` and expose them to Lua (`acapi.regRead`, `acapi.regWrite`).
	
## Phase 3.7: PolygonReducer Port to ArchiLua (Interactive PoC)
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

## Phase 4: Stability & Logic (The MVP)
- [ ] **GC Safety:** C++ side `collectgarbage("stop")` before ACAPI calls and `collectgarbage("collect")` on scope exit.
- [ ] **Transaction Exception Safety:** Ensure that if a Lua script throws an error between `beginundo` and `endundo`, the C++ host gracefully aborts/closes the open ACAPI transaction to prevent DB corruption.
- [ ] **Transformation Logic:** Emulate GDL-style `ADD`, `MUL`, `ROTX` stack within Lua for panel alignment.

---
### v0.1.0 - MVP REACHED

## Phase 5: Scaling (Post-MVP)
- [ ] **Userdata Migration:** Replace Lua tables with full userdata + metatables for element objects. Adds `__index` for lazy access, `__newindex` for writes, `__gc` for cleanup, type identity via metatable comparison.
- [ ] **GUI Integration:** `LUA-LIMGUI` (Dear ImGui) overlay for real-time parameter tweaking.
- [ ] **Event Listeners:** Lua callbacks triggered by ArchiCAD element modification events.
  - A wall modified can trigger a lua script again that was run on that wall.
- [ ] **Automated Header Export:** Python/Clang-AST script to batch-generate Lua bindings for the full AC API.
- [ ] **ArchiCAD 28/29:** support
- [ ] **SamuTeszt Hook:** JSON dump of Lua tables before/after placement for regression testing.

## Phase 6: Generalizing (Support other Languages) 
- [ ] **Python Interpreter Integration:** A Python (and later other languages) interpreter to be integrated
  - A common API interface / Bridge is to be defined, so that multiple language interpreters can be added later on

