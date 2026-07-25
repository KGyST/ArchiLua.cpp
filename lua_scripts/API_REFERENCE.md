# acapi.* — Lua API Reference

## Selection

`local guids = acapi.getSel()` — array of selected element GUID strings.

## Reading

`local wall = acapi.getWall(guid)` — wall table with fields: `guid`, `layer`, `type`, `height`, `thickness`, `begC{x,y}`, `endC{x,y}`, `coords[{x,y}]`, `openings{windows=[guid], doors=[guid]}`.

`local floorIdx = acapi.getCurrentFloor()` — index of the currently active story.

`local elem = acapi.get(guid)` — generic element table: `guid`, `layer`, `typeName`, `coords[{x,y}]`.

`local poly = acapi.getPoly(guid)` — polygon vertex array `[{x,y}]` or `nil`.

`local params = acapi.getParams(guid)` — GDL parameter table `{name=value}` for library-part-based elements (objects, doors, windows, etc.).

## Writing (undoable)

### Undo batching
```lua
acapi.beginUndo("label")   -- start batch
acapi.setWall(...)          -- queue changes
acapi.endUndo()             -- flush as single undo step
```

Without begin/end, each write call creates its own undo step.

### Functions

`acapi.setWall(guid, { ... })` — wall fields: `height`, `thickness`, `layer`, `begC{x,y}`, `endC{x,y}`.

`acapi.set(guid, { ... })` — generic: `layer` (common) + type-specific fields (TBD).

`acapi.setParams(guid, { name = value, ... })` — set GDL parameters by name.

`local libInd, name = acapi.findObject("name")` — search library part by name.

## Creation

`local guid = acapi.create(libInd, {x, y}, { name = value, ... })` — create object instance with optional initial params.

`local guid = acapi.addWall({ begC={x,y}, endC={x,y}, height, thickness, layer, floor })` — create a new straight wall. `floor` defaults to 1; also accepts `storey` as alias. Pass `poly` table (array of `{x,y}`) instead of `begC`/`endC` for a polygonal wall.

`local guid = acapi.addWindow(wallGuid, { objLoc, height, width, sillHeight, wallSide, mirrored })` — place a window in a straight wall. `wallSide` (`"inside"`/`"outside"`) and `mirrored` (bool) control orientation.

`local guid = acapi.addDoor(wallGuid, { objLoc, height, width, wallSide, mirrored })` — place a door in a straight wall. Same params as `addWindow` (no `sillHeight`).

`local guid = acapi.addSlab({ poly, thickness, layer, floor })` — create a polygonal slab. `poly` is an array of `{x,y}` vertices.

`local guid = acapi.addRoof({ poly, thickness, layer, floor })` — create a polygonal roof.
