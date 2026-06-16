# acapi.* — Lua API Reference

## Selection

`local guids = acapi.getsel()` — array of selected element GUID strings.

## Reading

`local wall = acapi.getwall(guid)` — wall table with fields: `guid`, `layer`, `type`, `height`, `thickness`, `begC{x,y}`, `endC{x,y}`, `coords[{x,y}]`, `openings{windows=[guid], doors=[guid]}`.

`local elem = acapi.get(guid)` — generic element table: `guid`, `layer`, `typeName`, `coords[{x,y}]`.

`local poly = acapi.getpoly(guid)` — polygon vertex array `[{x,y}]` or `nil`.

`local params = acapi.getparams(guid)` — GDL parameter table `{name=value}` for library-part-based elements (objects, doors, windows, etc.).

## Writing (undoable)

### Undo batching
```lua
acapi.beginundo("label")   -- start batch
acapi.setwall(...)          -- queue changes
acapi.endundo()             -- flush as single undo step
```

Without begin/end, each write call creates its own undo step.

### Functions

`acapi.setwall(guid, { ... })` — wall fields: `height`, `thickness`, `layer`, `begC{x,y}`, `endC{x,y}`.

`acapi.set(guid, { ... })` — generic: `layer` (common) + type-specific fields (TBD).

`acapi.setparams(guid, { name = value, ... })` — set GDL parameters by name.

`local libInd, name = acapi.findobject("name")` — search library part by name.

## Creation

`local guid = acapi.create(libInd, {x, y}, { name = value, ... })` — create object instance with optional initial params.

`local guid = acapi.addwall({ begC={x,y}, endC={x,y}, height, thickness, layer, storey })` — create a new wall element. `storey` defaults to 1 (first floor); also accepts `floorInd` as alias.
