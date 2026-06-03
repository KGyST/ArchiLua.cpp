# ADR: Element Type Name Mapping Strategy

- Status: Accepted
- Date: 2026-06-03
- Context: The `ElementTypeName()` function uses a C++ switch to map `API_ElemTypeID` enum values to display strings. We evaluated moving this to an external JSON file so adding AC28/29 types wouldn't require a rebuild.
- Decision: **Keep the C++ switch for type-name mapping. Add `acapi.getparams(guid)` for generic parameter access.**

  Alternatives considered:

  1. **JSON file with enum integer keys** — rejected. `API_ElemTypeID` integer values are not guaranteed stable across AC versions. A JSON with `[1] = "Wall"` would silently produce wrong names on AC28 if enum values shift.

  2. **JSON file with string enum-name keys** (e.g. `"API_WallID"`) — rejected. Requires a reverse C++ string→enum map at runtime. That map is itself a switch/array that must be kept in sync with AC headers — same maintenance burden as the original switch, plus a JSON parser dependency.

  3. **Compile-time code generation** (Python script reading JSON → emits .hpp) — rejected. Adds build toolchain dependency, breaks the two-stage build (cmake → msbuild). Risk: generated file out of sync with headers.

  4. **Keep C++ switch** — chosen. Compile-time checked by `/W4` / `-Wswitch`: the compiler warns about any unhandled `API_ElemTypeID` values. When AC28 adds new types, the switch emits a warning at build time. Zero runtime failure mode. Zero dependencies.

  5. **`acapi.getparams(guid)` via `ACAPI_Element_GetParams`** — chosen as the generic, future-proof access path. Returns ALL GDL-style parameters as a Lua table `{name = value, ...}` for any element type. No per-type C++ code needed. Works identically on AC27/28/29 because `ACAPI_Element_GetParams` is a stable API.

- Pro/Con Analysis:
  - **Keep C++ switch**: Pro: compiler-verified, zero runtime risk, zero deps. Con: adding a new type name requires a C++ rebuild.
  - **JSON + reverse map**: Pro: type names editable without rebuild. Con: reverse map is a C++ maintenance burden, JSON parser dep, runtime failure mode if file is missing/corrupt, no compiler verification.
  - **`acapi.getparams()`**: Pro: works for ALL types without per-type C++, stable across AC versions, gives access to the same parameters GDL sees (PosX, Height, etc.). Con: only returns GDL parameters, not raw C struct fields (those still need per-type C++ functions like `GetWall`).

- Exit Strategy: If a future AC version dramatically changes `ACAPI_Element_GetParams`, the function will fail gracefully (`lua_pushnil` + error string). The C++ type-name switch will be updated as part of the AC version migration (a compile-time warning will flag it).
