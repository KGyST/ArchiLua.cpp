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

`acapi.set(guid, { ... })` — generic: `layer` (common) + `pos = {x, y}` for objects.

`acapi.delete(guid)` — delete an element (single undo step). Returns `true` on success.

`local guids = acapi.createMany(libInd, {{x, y[, angle]}, ...}, {params})` — place N objects in ONE undo step. Returns guid array.

Undo batching: `beginUndo(label)` … `endUndo()` merges `set`/`setparams`/`delete` into one step (byte-wise mask merge, latest call wins; deletes replay first). `create`/`createMany` run their own command (guids needed immediately).

`acapi.setParams(guid, { name = value, ... })` — set GDL parameters by name.

`local libInd, name = acapi.findObject("name")` — search library part by document name. The match is verified case-insensitively; a mismatch fails loudly instead of placing the wrong part.

## Creation

`local guid = acapi.create(libInd, {x, y[, angle]}, { name = value, ... })` — create object instance with optional initial params (`angle` in radians). Only `APILib_ObjectID` parts supported (lib type ≠ element type — mapped explicitly). Defaults (layer etc.) come from `GetDefaults`; a zeroed header fails with `APIERR_BADINDEX`.

`local list = acapi.listParams(libInd)` — diagnostic: `{name, typeID, typeMod, value}` per part parameter (`typeMod`: 0 = simple/settable, 2 = array).

`local guid = acapi.addWall({ begC={x,y}, endC={x,y}, height, thickness, layer, floor })` — create a new straight wall. `floor` defaults to 1; also accepts `storey` as alias. Pass `poly` table (array of `{x,y}`) instead of `begC`/`endC` for a polygonal wall.

`local guid = acapi.addWindow(wallGuid, { objLoc, height, width, sillHeight, refSide, oSide, mirrored })` — place a window in a straight wall. `refSide` and `oSide` (each `"inside"`/`"outside"`, mapping 1:1 to the stored mirror flags) and `mirrored` (bool) control orientation.

`local win = acapi.getWindow(guid)` — window table: `guid`, `wallGuid`, `objLoc`, `height`, `width`, `sillHeight`, `refSide`/`oSide` (`"inside"`/`"outside"`), `mirrored`, `openingAngle`.

`acapi.setWindow(guid, { ... })` — update window fields (same keys as `getWindow`). Only actually-changed fields are written, so saving unchanged values is a no-op and never mirrors the window.

`local guid = acapi.addDoor(wallGuid, { objLoc, height, width, refSide, oSide, mirrored })` — place a door in a straight wall. Same params as `addWindow` (no `sillHeight`).

`local guid = acapi.addSlab({ poly, thickness, layer, floor })` — create a polygonal slab. `poly` is an array of `{x,y}` vertices.

`local guid = acapi.addRoof({ poly, thickness, layer, floor })` — create a polygonal roof.

## Registry (persist GUI values)

`local v = acapi.regRead(section, key[, default])` — read a string from `HKCU\Software\Samu\ArchiLua\<section>`. Returns `default` (or `nil`) when missing. Section/key must match `[A-Za-z0-9_]{1,64}`, so scripts cannot escape the sandbox.

`acapi.regWrite(section, key, value)` — write a string/number/boolean (stored as `REG_SZ`, max 4000 chars) under `HKCU\Software\Samu\ArchiLua\<section>`. Returns `true` on success.
