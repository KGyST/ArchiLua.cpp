# ArchiLua Core

## 1. Ownership & Lifecycle
* **Lua State Singleton:** Created during `ACAPI_Initialize` and destroyed at `ACAPI_Free`. The state is persistent throughout the session.
* **Execution Lock:** All Lua calls must run exclusively on the ArchiCAD Main Thread.
* **Error Handling:** Mandatory use of Lua `pcall` (protected call). Lua-side errors are caught by C++ and logged via `ACAPI_WriteReport` (in red/error style).

## 2. Data Marshalling Strategy
* **Value Copy vs. Userdata:**
    * **Geometry (Coords, Memos):** Converted to pure Lua Tables (Deep Copy). No live references to internal API handles are kept in Lua to ensure memory safety.
    * **GUIDs:** Handled as `std::string` on the Lua side.
* **Handles:** ArchiCAD `GSHandle` and `GSPtr` types are managed exclusively by the C++ wrapper. Lua never sees raw pointers.

## 3. Transaction Management
* **Undo Safety:** Lua scripts cannot call modifying commands directly. The C++ wrapper provides an `ArchiLua.DoUndoable(function)` wrapper to ensure the script's entire execution is wrapped in a single `ACAPI_CallUndoableCommand` block.
* **Garbage Collection (GC) Policy:**
    * The wrapper calls `lua_gc(L, LUA_GCSTOP, 0)` before any ACAPI transaction.
    * Upon completion or script termination, `LUA_GCRESTART` and a forced `LUA_GCCOLLECT` are executed.

## 4. Module Map
- `Selection`: Retrieve GUIDs of currently selected elements.
- `Element`: Read parameters (Get), identify element types.
- `Geometry`: Convert `API_ElementMemo` data (Polygon, Hole, Edge) into Lua tables.
- `Builder`: Handle `ACAPI_Element_Create` calls and GDL parameter mapping (`API_AddParID`).

