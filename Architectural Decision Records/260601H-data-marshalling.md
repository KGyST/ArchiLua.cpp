# ADR: Data Marshalling Strategy — Table vs Userdata

- Status: Accepted
- Date: 2026-06-01
- Context:
  Phase 2.5 implements element reading (`acapi.get`, `acapi.getpoly`, `acapi.getparams`)
  by serializing C struct fields into Lua tables.
  Tables are simple and require zero infrastructure,
  but they allocate eagerly on every call, carry no methods,
  hold no back-reference to the original C struct,
  and cannot distinguish between element types at the language level.

  The next phase (Phase 3) needs a richer object model:
  lazy field access, attached methods (`:poly()`, `:params()`),
  type identity, and eventually write-back (`elem.field = value`).
- Decision:
  **Phase 2.5 keeps Lua tables — Phase 3 migrates to userdata with metatables.**

  Alternatives considered:

  1. **Pure Lua tables** — chosen for Phase 2.5.
     Every `acapi.get(guid)` call allocates a table and copies all fields.
     The C struct is discarded after the table is built.
     No metatable, no methods, no back-reference.

  2. **Full userdata with metatable** — chosen for Phase 3.
     `acapi.get(guid)` returns a `full userdata` that holds a pointer
     to an `API_Element` snapshot (or a smart handle).
     `__index` dispatches field reads to C getters.
     Methods like `:poly()` and `:params()` live on the metatable.
     `__gc` frees the C resource.

  3. **Lightuserdata + registry table** — rejected.
     Store `API_Guid` as lightuserdata, keep all data in a C-side registry table.
     Pro: no GC overhead, no allocation on access.
     Con: lifetime management is manual (must purge registry on element delete),
     no `__gc`, and every access still requires a C function call.
     Offers no real advantage over full userdata with less safety.

  4. **Hybrid: table with attached metatable** — rejected as intermediate step.
     Create a table, set a metatable with `__index` that calls C functions,
     but still pre-populate the table with eager fields.
     Combines the worst of both: eager allocation cost PLUS metatable overhead
     for the same API surface. No benefit over either pure approach.

- Pro/Con Analysis:
  - **Pure tables:**
    Pro: zero infrastructure, no GC registration, trivial to debug,
    fields are plain Lua values inspectable in any Lua tool.
    Con: eager allocation on every call, no lazy field access,
    no runtime type identity, cannot attach methods.
  - **Full userdata:**
    Pro: lazy evaluation (fields read via `__index` only when accessed),
    methods live on metatable (no need for `acapi.getpoly(elem)` — write `elem:poly()`),
    type identity via metatable comparison (`getmetatable(elem) == WallMT`),
    `__gc` for automatic resource cleanup.
    Con: metatable infrastructure (registry table, type-dispatch in `__index`),
    GC pressure from full userdata blocks, more C++ code,
    harder to debug (userdata is opaque in `print()` without `__tostring`).
  - **Lightuserdata + registry:**
    Pro: smallest allocation, fastest pointer access.
    Con: no metatables, no `__gc`, manual lifetime management,
    memory leak if script errors before registry cleanup.
  - **Hybrid:**
    Pro: none.
    Con: all the cost of tables plus all the complexity of metatables.

- Exit Strategy:
  Userdata is the permanent target.
  Tables are a deliberate Phase 2.5 simplification to ship reading
  without infrastructure overhead.
  When Phase 3 begins, `APIModule.hpp` will gain a new `GetElementUserdata`
  alongside the current `GetElement`, and the metatable dispatch code
  will live in a new file (`src/API/ElementMeta.hpp`).
  The table path is deleted once userdata covers all call sites,
  not maintained in parallel.
