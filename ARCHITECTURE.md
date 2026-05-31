ArchiLua is a Lua interpreter embedded into an ArchiCAD plugin.

# ArchiLua

ArchiCAD API <-> Lua 5.4 bridge using `sol2`. Designed to replace GDL logic with C++ speed and Lua flexibility.

## Core Setup
* **ArchiCAD API version** ArchiCAD SE 2024 (ArchiCAD 27)
* **Tech:** C++17, sol2 (header-only), Lua 5.4.
* **Architecture:** In-process embedding. No external Python runtime dependencies.

## MVP Scope
* `Selection.GetWall()`: Returns wall GUIDs and `API_ElementMemo` (geometry) as Lua tables.
* `Object.Place()`: Instantiates GDL parts (e.g., `m_Viapanel_Wallpanel`) with dynamic parameters.
* **Memory:** Manual GC management (`collectgarbage("stop/collect")`) during ACAPI transactions.

## Project Structure
* `/src`: C++ Wrappers (sol2) & ACAPI interfaces.
* `/scripts`: `try_*.lua` - Playful logic for panel distribution.
* `../CommonLibs.cpp` - custom functions to include when needed
* `../support/archicad-buildsupport/AC27/API.6003/Support/Modules` - ArchiCAD headers, include and object files

## Workflow
1. Fetch selected wall geometry.
2. Calculate panel layout in Lua (skipping openings).
3. Batch-create GDL elements via `ArchiLua.PlaceObject`.