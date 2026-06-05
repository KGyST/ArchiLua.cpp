ArchiLua is a Lua interpreter embedded into an ArchiCAD plugin.

# ArchiLua

ArchiCAD API <-> Lua 5.4 bridge (raw C API).

## Core Setup
* **ArchiCAD API version** ArchiCAD SE 2024 (ArchiCAD 27)
* **Tech:** C++17, raw Lua C API, Lua 5.4.
* **Architecture:** In-process embedding. No external Python runtime dependencies.

## MVP Scope
* `Selection.GetWall()`: Returns wall GUIDs and `API_ElementMemo` (geometry) as Lua tables.
* `Object.Place()`: Instantiates GDL parts (e.g., `m_Viapanel_Wallpanel`) with dynamic parameters.
* **Memory:** Manual GC management (`collectgarbage("stop/collect")`) during ACAPI transactions.

## Project Structure
* `/src`: C++ bridge (raw Lua C API) & ACAPI interfaces.
* `/scripts`: `try_*.lua` - Playful logic for panel distribution.

## Workflow
1. Fetch selected wall geometry.
2. Calculate panel layout in Lua (skipping openings).
3. Batch-create GDL elements via `ArchiLua.PlaceObject`.