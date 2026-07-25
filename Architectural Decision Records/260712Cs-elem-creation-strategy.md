# ADR: Element Creation Strategy (Phase 3.5)

- Status: Accepted
- Date: 2026-07-12
- Context: Adding creation endpoints for walls (straight + polygonal),
  windows/doors (marker-based sub-elements), slabs, and roofs.
- Decision:

  **Straight elements** (walls with `begC`/`endC`)
  Use `ACAPI_Element_Create(&elem, nullptr)` — no memo needed.
  Straight walls have no polygon memo; position is set via
  `elem.wall.begC`/`endC`.

  **Polygonal elements** (polygonal walls, slabs, roofs)
  Use `ACAPI_Element_Create(&elem, &memo)` with `memo.coords`
  allocated via `BMAllocateHandle`. The polygon is closed by
  duplicating the first vertex at index `nVerts`.
  For walls, `elem.wall.type = APIWtyp_Poly` must be set.
  For slabs/roofs, all elements are implicitly polygonal.

  **Marker-based sub-elements** (windows, doors)
  Use `ACAPI_Element_CreateExt(&elem, &memo, 1UL, &marker)`
  with the marker library part resolved via:
  `ACAPI_Element_GetDefaultsExt` →
  `ACAPI_LibraryPart_GetMarkerParent` →
  `ACAPI_LibraryPart_Search` →
  `ACAPI_LibraryPart_GetParams`.
  The marker's `subElem.object.pen` and `useObjPens` are set
  to match ArchiCAD defaults.

  **Standard map:**
  | Element | API type | Create call | Memo required |
  |---|---|---|---|
  | Wall (straight) | `API_WallID` | `Create` | No |
  | Wall (poly) | `API_WallID` | `Create` | `coords` |
  | Window | `API_WindowID` | `CreateExt` | `coords` + marker |
  | Door | `API_DoorID` | `CreateExt` | `coords` + marker |
  | Slab | `API_SlabID` | `Create` | `coords` |
  | Roof | `API_RoofID` | `Create` | `coords` |

  **Wall side + mirrored mapping:**
  `wallSide = "inside"` or numeric `1` →
  `elem.{window,door}.wallSide = 1`.
  `wallSide = "outside"` or numeric `0` →
  `elem.{window,door}.wallSide = 0`.
  `mirrored = true` → `elem.{window,door}.reflected = true`.

  **Alternatives considered:**
  1. Using `ACAPI_Element_CreateExt` for all elements — rejected
     because straight walls and simple slabs have no sub-elements.
  2. Using `ACAPI_LibraryPart_GetParams` for all openings to set
     custom opening shapes — deferred to post-MVP. Current
     implementation uses defaults from the maker library part.
  3. Creating a shared `ReadPolyFromLua` helper — deferred.
     The poly reading logic is duplicated across `addWall`,
     `addSlab`, `addRoof`; a helper could be extracted later.

- Pro/Con Analysis:

  **Per Alternative 1 (straight walls with memo):**
  Pros: Uniform code path, no special cases.
  Cons: Wastes memory, may cause API warnings for null coords
  on straight walls. No benefit.

  **Per Alternative 2 (custom opening shapes):**
  Pros: Full control over window/door geometry.
  Cons: Requires deep understanding of opening profile params.
  Adds complexity for little MVP value.

  **Per Alternative 3 (shared helper):**
  Pros: DRY, less code.
  Cons: Requires either a template or passing multiple out-params
  (handle, vertex count, error string) which adds complexity for
  only 3 call sites. Easy to extract later.

- Exit Strategy: N/A — no mock/stub involved.
