-- try_dividers.lua — N standalone divider points on a wall, kept in sync via the observer.
-- Dividers are independent library objects on the wall centerline (visible 2D plan points).
-- On wall Change/Edit their XY positions are recomputed from the new begC/endC.

-- Bump on every script change; shown in the dialog footer to verify what's running.
local SCRIPT_VER = "260922h"

local watchedWall = nil
local dividers = {} -- divider entries {guid, angle}, in wall order (dense 1..N)
local rowMeta = {} -- row identity: {libInd, partName} (kept OUT of the array
                   -- so ipairs traversals can never trip over hash keys)
local DIV_COUNT_DEFAULT = 10
local REG_SEC = "try_dividers"
-- Live row's count (set at place/adopt). The UI Divisions field re-divides
-- the live row immediately (replace flow); without a live row it applies on
-- next Place. Slot mapping never shifts except through replace.
local ROW_N = DIV_COUNT_DEFAULT
local WATCHFUNC = "try_dividers.lua\\onDividersWallEvent"
local ROW_KEY = "try_dividers.lua" -- ledger namespace: this script owns only
                                   -- userData[ROW_KEY]; foreign keys are preserved
                                   -- on every write, never read

-- Panel options (persisted). b = nil means auto = wall thickness.
local opts = { b = nil, zzyzx = 0.25, center = false, count = DIV_COUNT_DEFAULT }

local function clampCount(v)
    v = tonumber(v)
    if v == nil then return nil end
    v = math.floor(v)
    if v < 1 or v > 100 then return nil end
    return v
end

local function loadOpts()
    opts.b = tonumber(acapi.regRead(REG_SEC, "B")) -- nil stays nil = auto
    opts.zzyzx = tonumber(acapi.regRead(REG_SEC, "ZZYZX")) or 0.25
    opts.center = acapi.regRead(REG_SEC, "center", "false") == "true"
    opts.count = clampCount(acapi.regRead(REG_SEC, "DIVCOUNT")) or DIV_COUNT_DEFAULT
end

local function saveOpts()
    acapi.regWrite(REG_SEC, "B", opts.b ~= nil and tostring(opts.b) or "")
    acapi.regWrite(REG_SEC, "ZZYZX", tostring(opts.zzyzx))
    acapi.regWrite(REG_SEC, "center", opts.center and "true" or "false")
    acapi.regWrite(REG_SEC, "DIVCOUNT", tostring(opts.count))
end

local function effB(wall)
    return opts.b or wall.thickness
end

-- Forward declarations: these run before their definitions below
-- (Lua resolves locals lexically — without this they bind to nil globals).
local logEvent
local resolvePart
local lastRev -- entry revision mirror (see sync section)
local dirtyCause -- what dirtied the tick: "drag"/"undo"/"redo"/nil
local panelsDiverged -- defined late (needs wall helpers); guard callers above it
local lastSync -- session last-synced keys; repairRow (above) refreshes it
local syncKey -- key builder; defined late next to lastSync

-- Ownership ledger lives ON THE WALL (user data): userData[ROW_KEY] =
-- {children={{guid,angle[,dead]}}, partName, count, opts, wall={begC,endC}}.
-- Travelling with the element through save/load (user-data ops are undoable).
-- Per-wall params live in the entry; the registry holds only last-used
-- DEFAULTS prefilling the form for walls without an entry. Foreign keys
-- (other scripts' entries) are preserved on every write, never touched.
-- Legacy flat rows ({dividers, ...}) are read-tolerant, write-strict:
-- normalized on adopt, wiped on load.

-- Full-ledger fetch (no side effects). Returns a table (possibly empty).
local function readLedger(wallGuid)
    local ud = acapi.getUserData(wallGuid)
    if type(ud) == "table" then
        return ud
    end
    return {}
end

-- Own entry fetch. Normalizes legacy flat rows to entry shape.
-- Returns entry or nil.
local function readEntry(wallGuid)
    local ud = acapi.getUserData(wallGuid)
    if type(ud) ~= "table" then
        return nil
    end
    if type(ud[ROW_KEY]) == "table" then
        return ud[ROW_KEY]
    end
    if type(ud.dividers) == "table" then
        return {
            children = ud.dividers,
            partName = ud.partName,
            count = ud.count,
            opts = ud.opts,
        }
    end
    return nil
end

-- All owned guids in an entry (children or legacy dividers).
local function entryGuids(entry)
    local out = {}
    for _, e in ipairs(entry.children or entry.dividers or {}) do
        if type(e) == "table" and e.guid and e.guid ~= "" then
            out[#out + 1] = e.guid
        end
    end
    return out
end

-- rev classifies stack walks: the tick detects silent user-data undos/redos
-- (which notify nothing) by rev mismatch and adopts read-only. Bumped on
-- every effective persist, skipped on no-op persists (change-detected).
local function buildEntry(wall, rev)
    local entry = { children = {}, partName = rowMeta.partName, count = ROW_N,
                    rev = rev, opts = { zzyzx = opts.zzyzx, center = opts.center } }
    if opts.b ~= nil then entry.opts.b = opts.b end
    for i, d in ipairs(dividers) do
        if type(d) == "table" and d.guid then
            local e = { guid = d.guid, angle = d.angle or 0 }
            if d.dead then e.dead = true end
            entry.children[i] = e
        end
    end
    if wall and wall.begC and wall.endC then
        entry.wall = { begC = { x = wall.begC.x, y = wall.begC.y },
                       endC = { x = wall.endC.x, y = wall.endC.y } }
    end
    return entry
end

-- Read-modify-write of OWN entry only (foreign keys preserved).
-- Legacy residue is replaced (its children were wiped at load).
-- Change-detected: an identical entry skips the write (no gratuitous unit —
-- every skipped write is a redo stack saved, and unchanged persists are the
-- ones that clobber concurrent writers with stale blobs).
-- Display-only: JSON round-trips integers as floats ("rev=2.0"); show whole
-- numbers without the .0 (storage and arithmetic untouched).
local function revStr(v)
    local n = tonumber(v)
    if n == nil then return "none" end
    local i = math.tointeger(n)
    if i ~= nil then return tostring(i) end
    return tostring(n)
end

local function numEq(a, b)
    if a == nil and b == nil then return true end
    if a == nil or b == nil then return false end
    return math.abs(tonumber(a) - tonumber(b)) < 1e-9
end

local function sameEntry(a, b) -- semantic compare, ignoring rev/wall stamps
    if type(a) ~= "table" or type(b) ~= "table" then return false end
    if a.partName ~= b.partName then return false end
    if not numEq(a.count, b.count) then return false end
    local ao, bo = a.opts or {}, b.opts or {}
    if not numEq(ao.zzyzx, bo.zzyzx) then return false end
    if (ao.center and true or false) ~= (bo.center and true or false) then return false end
    if not numEq(ao.b, bo.b) then return false end
    local ac, bc = a.children or {}, b.children or {}
    if #ac ~= #bc then return false end
    for i, e in ipairs(ac) do
        local f = bc[i]
        if type(f) ~= "table" or e.guid ~= f.guid then return false end
        if not numEq(e.angle or 0, f.angle or 0) then return false end
        if (e.dead and true or false) ~= (f.dead and true or false) then return false end
    end
    return true
end

local function persistState(wall)
    if not watchedWall then return true end
    local ledger = readLedger(watchedWall)
    if type(ledger.dividers) == "table" and ledger[ROW_KEY] == nil then
        ledger = {} -- legacy residue: own entry starts clean
    end
    if wall == nil then
        wall = acapi.getWall(watchedWall)
    end
    local prev = ledger[ROW_KEY]
    local prevRev = (type(prev) == "table" and tonumber(prev.rev)) or 0
    local entry = buildEntry(wall, prevRev + 1)
    if sameEntry(entry, prev) then
        lastRev = prevRev -- no-op persist: session already mirrors storage
        return true
    end
    ledger[ROW_KEY] = entry
    local ok, err = acapi.setUserData(watchedWall, ledger)
    if not ok then
        logEvent("user-data save failed: " .. tostring(err))
        return ok
    end
    lastRev = prevRev + 1
    return ok
end

-- Remove OWN entry, keep the objects. Preserves foreign keys; deletes the
-- user data only when nothing else remains.
local function clearEntry(wallGuid)
    if not wallGuid then return true end
    local ledger = readLedger(wallGuid)
    local hadLegacy = type(ledger.dividers) == "table" and ledger[ROW_KEY] == nil
    ledger[ROW_KEY] = nil
    if hadLegacy then
        ledger.dividers = nil
        ledger.partName = nil
        ledger.count = nil
        ledger.opts = nil
        ledger.libInd = nil
    end
    local empty = true
    for _ in pairs(ledger) do empty = false break end
    if empty then
        return acapi.deleteUserData(wallGuid)
    end
    local ok, err = acapi.setUserData(wallGuid, ledger)
    if not ok then
        logEvent("user-data clear failed: " .. tostring(err))
    end
    return ok
end

local function pushOptsToForm()
    if opts.b ~= nil then
        ExecuteJS(string.format("document.getElementById('panelB').value='%s';", tostring(opts.b)))
    else
        ExecuteJS("document.getElementById('panelB').value='';")
    end
    ExecuteJS(string.format("document.getElementById('panelZZ').value='%s';", tostring(opts.zzyzx)))
    ExecuteJS(string.format("document.getElementById('centerDiv').checked=%s;",
        opts.center and "true" or "false"))
    ExecuteJS(string.format("document.getElementById('divCount').value='%d';", opts.count))
    -- Adopted part name follows the row (the field may hold a stale typed value).
    ExecuteJS(string.format("document.getElementById('partName').value='%s';",
        tostring(rowMeta.partName or ""):gsub("'", "")))
end

-- Resolve a fresh libInd (indices are session-volatile); fails loudly when
-- the part left the library. Legacy rows carry libInd only (no partName):
-- used as-is, may go stale across reloads — re-place then.
local function resolveRowPart(row)
    if type(row.partName) == "string" and row.partName ~= "" then
        local libInd, err = resolvePart(row.partName)
        if not libInd then
            return nil, err
        end
        return libInd
    end
    local legacy = tonumber(row.libInd)
    if legacy ~= nil then
        return legacy
    end
    return nil, "row has neither partName nor libInd — re-place"
end

-- Adopt a wall's ledger entry into session state (table + opts + form).
-- First contact only: with keepOpts (refresh path) the divider identity
-- (guids, part, count) is adopted but live session opts (just typed in the
-- form) win — otherwise every refresh would revert the fresh edits to the
-- stored copy. Full adopt (place/repair/readonly paths) takes entry.opts as
-- truth and never writes them back to the registry: registry holds pre-wall
-- defaults only. Slot-density invariant: stored children are always valid
-- tables (buildEntry guarantees it), so the indexed fill below never leaves
-- holes that would truncate the ipairs traversals elsewhere.
local function adoptRow(wallGuid, row, keepOpts)
    dividers = {}
    for i, e in ipairs(row.children or row.dividers or {}) do
        if type(e) == "table" and e.guid and e.guid ~= "" then
            dividers[i] = { guid = e.guid, angle = tonumber(e.angle) or 0 }
            if e.dead then dividers[i].dead = true end
        end
    end
    local libInd, err = resolveRowPart(row)
    if not libInd then
        return false, err
    end
    rowMeta.libInd = libInd
    rowMeta.partName = row.partName -- stable identity; libInd re-resolves per refresh
    ROW_N = tonumber(row.count) or #dividers
    opts.count = ROW_N -- session follows the live row; registry keeps the
                       -- pre-wall default for walls without a row
    local er = tonumber(row.rev)
    if er ~= nil then lastRev = er end -- session tracks the entry revision
    if not keepOpts and type(row.opts) == "table" then
        if row.opts.b ~= nil then opts.b = tonumber(row.opts.b) end
        if row.opts.zzyzx ~= nil then opts.zzyzx = tonumber(row.opts.zzyzx) or opts.zzyzx end
        if row.opts.center ~= nil then opts.center = (row.opts.center == true) end
        pushOptsToForm()
    end
    watchedWall = wallGuid
    return true
end

-- Mark entries currently unreachable (undo in flight, mid-drag states).
-- Missing entries are SKIPPED by ops but KEPT: only an explicit kind="delete"
-- (true deletion in plan), Delete Points, or re-place drops them — undo/redo
-- tennis may restore identical guids, and dropping on first miss orphaned
-- the row permanently. Delete batches stay safe (no dead guids flushed).
local function markStale()
    local missing = 0
    for _, d in ipairs(dividers) do
        if type(d) == "table" and d.guid then
            if acapi.get(d.guid) then
                d.missing = nil
            elseif not d.missing then
                d.missing = true
                missing = missing + 1
            end
        end
    end
    if missing > 0 then
        logEvent("suspect stale: " .. missing .. " unreachable (kept for redo)")
    end
end

local function liveCount()
    local n = 0
    for _, d in ipairs(dividers) do
        if type(d) == "table" and d.guid and not d.missing and not d.dead then
            n = n + 1
        end
    end
    return n
end

-- Detach wall + all panel watches (safe no-ops for dead guids).
local function unwatchRow()
    if watchedWall then
        acapi.unwatch(watchedWall)
    end
    for _, d in ipairs(dividers) do
        if type(d) == "table" and d.guid then
            acapi.unwatch(d.guid)
        end
    end
end

-- Merge form values (all optional) into opts; persists.
local function applyFormArgs(args)
    if not args then return end
    if args.b ~= nil then
        if args.b == "" then
            opts.b = nil
        elseif tonumber(args.b) then
            opts.b = tonumber(args.b)
        end
    end
    if args.zzyzx ~= nil and tonumber(args.zzyzx) then
        opts.zzyzx = tonumber(args.zzyzx)
    end
    if args.center ~= nil then
        opts.center = (args.center == true)
    end
    saveOpts()
end

local function wallPoint(wall, t)
    return {
        x = wall.begC.x + (wall.endC.x - wall.begC.x) * t,
        y = wall.begC.y + (wall.endC.y - wall.begC.y) * t
    }
end

local function wallAngle(wall)
    return math.atan(wall.endC.y - wall.begC.y, wall.endC.x - wall.begC.x)
end

-- Division point. With center enabled the panel midpoint (not its origin)
-- sits on the division: shifted back half of A along the wall direction
-- (panel local +X follows the wall vector via angle).
local function divPoint(wall, spacing, ang, i)
    local p = wallPoint(wall, (i - 0.5) / ROW_N)
    if opts.center then
        local dx = wall.endC.x - wall.begC.x
        local dy = wall.endC.y - wall.begC.y
        local len = math.sqrt(dx * dx + dy * dy)
        if len > 0 then
            p.x = p.x - dx / len * spacing / 2
            p.y = p.y - dy / len * spacing / 2
        end
    end
    p.angle = ang
    return p
end

-- Geometry helpers. Defined here (before placeDividers) because Lua binds
-- locals at definition time: users below must see them lexically.
local function geomHash(wall)
    return string.format("%.6f,%.6f,%.6f,%.6f",
        wall.begC.x, wall.begC.y, wall.endC.x, wall.endC.y)
end

local function wallLength(wall)
    return math.sqrt((wall.endC.x - wall.begC.x) ^ 2 + (wall.endC.y - wall.begC.y) ^ 2)
end

local function rowParams(wall, spacing)
    return { A = spacing, B = effB(wall), ZZYZX = opts.zzyzx }
end

resolvePart = function(name)
    if not name or name == "" then
        return nil, "enter a library part name first"
    end
    local libInd, foundName = acapi.findObject(name)
    if not libInd then
        return nil, foundName -- error string
    end
    return libInd, foundName
end

local function placeDividers(wallGuid, partName)
    local libInd, nameOrErr = resolvePart(partName)
    if not libInd then
        return false, nameOrErr
    end
    local foundName = nameOrErr
    local wall = acapi.getWall(wallGuid)
    if not wall then
        return false, "getWall failed"
    end
    local wallLen = wallLength(wall)
    local spacing = wallLen / ROW_N -- panel X size tiles the wall exactly
    local params = rowParams(wall, spacing)
    local ang = wallAngle(wall)
    local positions = {}
    for i = 1, ROW_N do
        positions[i] = divPoint(wall, spacing, ang, i)
    end
    -- one undoable command for the whole row
    local guids, err = acapi.createMany(libInd, positions, params)
    if not guids then
        return false, err
    end
    local info = err -- 2nd return is the "N placed, M overrides" info string
    dividers = {}
    for i, g in ipairs(guids) do
        dividers[i] = { guid = g, angle = ang }
    end
    -- stash for rebuilds on rotation change (libInd is session-ephemeral;
    -- partName below is the stable identity re-resolved on every refresh)
    rowMeta.libInd = libInd
    rowMeta.partName = partName
    persistState(wall)
    return true, foundName .. " (" .. tostring(info) .. ")"
end

local function refreshDividers(wallGuid)
    local wall = acapi.getWall(wallGuid)
    if not wall then
        return false, "wall not found"
    end
    local row = readEntry(wallGuid)
    if row then
        -- Identity adopt: session/form opts (fresh edits) win over the stored
        -- copy; the entry is rewritten with them at the end of this refresh.
        local aok, aerr = adoptRow(wallGuid, row, true)
        if not aok then
            return false, aerr
        end
    elseif #dividers == 0 then
        return false, "no dividers tracked (place first)"
    end
    markStale()
    if liveCount() == 0 then
        if #dividers == 0 then
            return false, "no dividers tracked (place first)"
        end
        return false, "all dividers unreachable (redo may restore them; Refresh repairs, Delete Points discards)"
    end
    local spacing = wallLength(wall) / ROW_N
    local newAngle = wallAngle(wall)
    local params = rowParams(wall, spacing)
    -- Partition: moves apply in place, direction changes need a rebuild
    -- (angle is not Change-editable). One syncRow call does moves + deletes
    -- + creates in a SINGLE undoable command (1 undo step total).
    -- Missing (in-flight) entries are skipped, never sent.
    local moves, dels, creates, createIdx = {}, {}, {}, {}
    for i, d in ipairs(dividers) do
        if not d.missing and not d.dead then
            local p = divPoint(wall, spacing, newAngle, i)
            if math.abs(newAngle - d.angle) > 1e-6 then
                table.insert(dels, d.guid)
                table.insert(creates, p)
                table.insert(createIdx, i)
            else
                table.insert(moves, { guid = d.guid, x = p.x, y = p.y })
            end
        end
    end
    local res, err = acapi.syncRow({
        moves = moves, del = dels, libInd = rowMeta.libInd,
        creates = creates, params = params
    })
    if not res then
        return false, tostring(err)
    end
    for _, g in ipairs(dels) do
        acapi.unwatch(g) -- deleted panels need no watch (safe no-op if auto-detached)
    end
    for k, i in ipairs(createIdx) do
        dividers[i] = { guid = res.created[k], angle = newAngle }
        acapi.watch(res.created[k], WATCHFUNC, { panel = true })
    end
    persistState(wall) -- guids may have changed on rotation rebuild
    return true
end

-- Manual repair (Refresh button only, never the tick): rebuild an orphaned
-- entry at CURRENT wall geometry. Deletes live remnants, re-places non-dead
-- slots via createMany in its own command, rewrites the entry. Tombstoned
-- slots (panels deliberately deleted in plan) stay empty. The wall is never
-- moved by the script; deliberate undos are never resurrected behind the
-- user's back (that would trap undo and duplicate on redo-tennis).
local function repairRow(wallGuid, entry)
    local libInd, err = resolveRowPart(entry)
    if not libInd then
        return false, err
    end
    local wall = acapi.getWall(wallGuid)
    if not wall then
        return false, "wall not found"
    end
    local wallLen = wallLength(wall)
    local spacing = wallLen / ROW_N
    local params = rowParams(wall, spacing)
    local ang = wallAngle(wall)
    markStale()
    local remnant = {}
    for _, d in ipairs(dividers) do
        if type(d) == "table" and d.guid and not d.missing and not d.dead and acapi.get(d.guid) then
            remnant[#remnant + 1] = d.guid
        end
    end
    if #remnant > 0 then
        acapi.beginUndo("Clear remnants")
        for _, g in ipairs(remnant) do
            acapi.delete(g)
        end
        local cok, cerr = acapi.endUndo()
        if not cok then
            return false, tostring(cerr)
        end
        for _, g in ipairs(remnant) do
            acapi.unwatch(g)
        end
    end
    local slots = entry.children or entry.dividers or {}
    local positions, slotIdx = {}, {}
    for i = 1, ROW_N do
        local child = slots[i]
        if not (type(child) == "table" and child.dead) then
            positions[#positions + 1] = divPoint(wall, spacing, ang, i)
            slotIdx[#slotIdx + 1] = i
        end
    end
    if #positions == 0 then
        return false, "all slots tombstoned — Delete Points to discard the row"
    end
    local guids, info = acapi.createMany(libInd, positions, params)
    if not guids then
        return false, info
    end
    dividers = {}
    for k, i in ipairs(slotIdx) do
        dividers[i] = { guid = guids[k], angle = ang }
        acapi.watch(guids[k], WATCHFUNC, { panel = true })
    end
    for i, child in ipairs(slots) do
        if type(child) == "table" and child.dead and not dividers[i] then
            dividers[i] = { guid = child.guid, angle = tonumber(child.angle) or 0, dead = true }
        end
    end
    rowMeta.libInd = libInd
    rowMeta.partName = entry.partName
    persistState(wall)
    lastSync[wallGuid] = syncKey(wall)
    return true, info
end

local eventLogCount = 0

logEvent = function(text)
    eventLogCount = eventLogCount + 1
    -- eventLog() is defined in the dialog HTML; falls back to result div
    ExecuteJS(string.format("if (typeof eventLog === 'function') { eventLog('#%d %s'); }",
        eventLogCount, text:gsub("'", "")))
end

-- Writes are refused inside element notifications (APIERR_REFUSEDCMD on nested
-- undoable commands — menu/button context is the only safe place). So wall
-- events only ATTEMPT a sync (harmless when allowed) and otherwise mark dirty;
-- the JS heartbeat (same safe context as button clicks) performs the real sync.
local syncDirty = false
local lastGuideTime = 0
-- Post-undo/redo inhibit: settling Change/Edit notifications arrive with
-- unchanged geometry right after a rollback; syncing them opens a new
-- transaction that wipes the redo stack for nothing. While armed, all
-- write-syncs become adopt-only. Armed on undo/redo events, disarmed by any
-- explicit user action (place/refresh/delete/unwatch) or a geometry change
-- (fresh drag); the timestamp lapses on its own so it cannot stick.
local inhibitUntil = 0
local inhibitGeom = nil
-- Entry revision mirror (bumped on every effective persist) + what dirtied
-- the tick. dirtyCause "undo"/"redo" (and silent rev mismatches) force the
-- read-only adopt path: opening any transaction there would wipe a pending redo.
-- (lastRev/dirtyCause forward-declared at top, initialized nil implicitly.)

-- Last synced wall state per guid (geometry + opts). The drop fires both a
-- flushed edit AND a change; the second run would redo identical work as its
-- own undo step (generic set() has no change detection), so it is skipped.
-- (forward-declared at top for repairRow; initialized here at load.)
lastSync = {}

syncKey = function(wall)
    return geomHash(wall) .. string.format(",%s,%s,%s,%s",
        tostring(ROW_N), tostring(opts.b), tostring(opts.zzyzx), tostring(opts.center))
end

local lastFailMsg = nil
local lastSuppressMsg = nil -- dedupe for the inhibit-suppressed line above

-- Tiny shared helpers (every user below binds locally).
local function watchPanels()
    for _, d in ipairs(dividers) do
        if type(d) == "table" and d.guid then
            acapi.watch(d.guid, WATCHFUNC, { panel = true })
        end
    end
end

local function disarmInhibit()
    inhibitUntil = 0 -- explicit user action ends any post-undo inhibit
    inhibitGeom = nil
end

local function reportSync(kind, ok, info)
    if ok then
        syncDirty = false
        dirtyCause = nil -- consumed: future ticks judge rev-mismatch on merits
        lastFailMsg = nil
        local msg = string.format("Dividers synced (%s): %d points", tostring(kind), #dividers)
        SetWebResult(msg)
        logEvent(msg)
        return
    end
    -- Transient refusal (nested command refused mid-drag, incl. empty-message
    -- outer refusals): park dirty, the heartbeat retries post-drop.
    local low = tostring(info):lower()
    if tostring(info):find("endUndo flush refused") or tostring(info) == "" or low:find("refus") then
        syncDirty = true
        local msg = "Sync deferred mid-drag (" .. tostring(kind) .. "), heartbeat will retry"
        SetWebResult(msg)
        logEvent(msg)
        return
    end
    local msg = "Dividers sync failed (" .. tostring(kind) .. "): " .. tostring(info)
    if msg == lastFailMsg then
        return -- identical repeat (e.g. tick on a dead row): retry stays silent
    end
    lastFailMsg = msg
    SetWebResult(msg)
    logEvent(msg)
end

-- Read-only adopt of the natively versioned entry (undo/redo legs and silent
-- rev mismatches). Full adopt (entry is time-traveled truth, form follows),
-- zero writes: opening any transaction here would wipe a pending redo.
-- Returns false only when adoption itself fails.
local function readonlyAdopt(tag)
    local entry = readEntry(watchedWall)
    if entry == nil then
        dividers = {}
        lastRev = nil
        logEvent(tag .. ": adopted absence (pre-place stack position)")
        return true
    end
    local aok, aerr = adoptRow(watchedWall, entry)
    if not aok then
        logEvent(tag .. " adopt failed: " .. tostring(aerr))
        return false
    end
    markStale()
    logEvent(string.format("%s: adopted (read-only) rev=%s (%d live)",
        tag, revStr(entry.rev), liveCount()))
    return true
end

-- Reverted-sync guidance (read-only): a moved-back sync means the wall undo
-- is next — rewriting it would trap wall-undo behind auto-redo. Shared by
-- the tick paths so the message stays identical everywhere.
-- No time-window gate: fresh drags always arrive with synchronous edit/change
-- events (direct try-sync + dirty retry), so unattributed divergence is always
-- rollback aftermath — guide, never rewrite. Writes require cause=="drag" or
-- a manual button; an earlier time-gated variant proved fall-through writes.
local function guideIfDiverged(wall)
    if panelsDiverged(wall) then
        if os.time() - lastGuideTime >= 10 then
            lastGuideTime = os.time()
            local msg = "Divider sync was undone — Ctrl+Z again undoes the wall move"
            SetWebResult(msg)
            logEvent(msg)
        end
        return true
    end
    return false
end

local function handleWallSync(guid, kind)
    if #dividers == 0 then
        -- Empty row: wall moves alone. Say so (silence here caused confusion);
        -- recovery is Redo (restores same guids) or re-place.
        local msg = "Wall moved but no dividers tracked — Redo, or Pick Wall + Place Points"
        SetWebResult(msg)
        logEvent("wall moved, row empty")
        return
    end
    local wall = acapi.getWall(guid)
    local key = wall and syncKey(wall) or nil
    if key ~= nil and lastSync[guid] == key then
        local msg = "Already in sync (" .. tostring(kind) .. "): " .. #dividers .. " points"
        SetWebResult(msg)
        logEvent("unchanged, skipped (" .. tostring(kind) .. ")")
        return
    end
    -- Post-undo inhibit: settling notifications carry post-rollback geometry;
    -- syncing them opens a unit that wipes redo for nothing. Suppress while
    -- geometry matches the latched snapshot; a changed geometry is a fresh
    -- drag (edit/change refresh the wall, undo/redo never do) → disarm and
    -- proceed, its redo wipe being standard and correct.
    if os.time() < inhibitUntil then
        local g = wall and wall.begC and wall.endC and geomHash(wall) or nil
        if inhibitGeom ~= nil and g ~= nil and g ~= inhibitGeom then
            inhibitUntil = 0
            inhibitGeom = nil
            lastSuppressMsg = nil
        else
            if inhibitGeom == nil then inhibitGeom = g end
            if readonlyAdopt("inhibit") then
                lastSync[watchedWall] = syncKey(wall)
            end
            local msg = "Sync suppressed (post-undo inhibit) — redo preserved"
            if msg ~= lastSuppressMsg then
                lastSuppressMsg = msg
                SetWebResult(msg)
                logEvent(msg)
            end
            return
        end
    else
        inhibitGeom = nil
    end
    lastSuppressMsg = nil
    local ok, info = refreshDividers(guid)
    if ok and key ~= nil then
        lastSync[guid] = key
    end
    reportSync(kind, ok, info)
end

function onDividersWallEvent(guid, kwargs, kind)
    if kind == "undo" or kind == "redo" then
        -- During undo/redo notifications NO ArchiCAD calls that write may run
        -- (not even getWall is touched here) — just flag with cause; the
        -- heartbeat adopts read-only (never syncs: any write wipes redo).
        syncDirty = true
        dirtyCause = kind
        if guid == watchedWall then
            inhibitUntil = os.time() + 4 -- settling writes suppressed (redo preserved)
            inhibitGeom = nil -- re-anchor on next gate hit (stack walks move geometry)
        end
        logEvent("undo/redo seen (" .. tostring(kind) .. "), will adopt read-only")
        return
    end
    if kind == "delete" then
        -- True deletion in plan (undo tennis arrives as undo/redo instead).
        if guid == watchedWall then
            unwatchRow()
            watchedWall = nil
            lastSync = {}
            lastRev = nil
            dirtyCause = nil
            syncDirty = false
            ExecuteJS("stopTick();")
            local msg = "Watched wall deleted — panels kept, re-pick a wall to re-watch"
            SetWebResult(msg)
            logEvent(msg)
        else
            for _, d in ipairs(dividers) do
                if type(d) == "table" and d.guid == guid and not d.dead then
                    d.dead = true -- tombstone: keeps slot mapping, skipped by ops
                    persistState()
                    logEvent("panel deleted in plan, slot kept empty")
                    break
                end
            end
        end
        return
    end
    if kind ~= "edit" and kind ~= "change" then
        return
    end
    if guid ~= watchedWall then
        return -- a panel's own edit/change; the row follows the wall only
    end
    dirtyCause = "drag" -- fresh gesture: writes allowed (no redo can be pending)
    handleWallSync(guid, kind)
end

-- Called by the JS heartbeat (800 ms while watching). Same safe context as a
-- button click, so commands are allowed here. Retries refused drag syncs and
-- adopts natively versioned state after undo/redo (read-only — writes would
-- wipe a pending redo); silent user-data walks surface via rev mismatch.
-- A reverted sync (panels moved back, wall unchanged) is GUIDED, never
-- rewritten: rewriting it would trap the user (wall-undo unreachable behind
-- an auto-redoing sync), so we only explain the next Ctrl+Z.
panelsDiverged = function(wall)
    local dx = wall.endC.x - wall.begC.x
    local dy = wall.endC.y - wall.begC.y
    local spacing = math.sqrt(dx * dx + dy * dy) / ROW_N
    local ang = wallAngle(wall)
    for i, d in ipairs(dividers) do
        local want = divPoint(wall, spacing, ang, i)
        local got = acapi.getPos(d.guid)
        if type(got) ~= "table" then
            return false -- unreadable (stale?) — prune path owns this case
        end
        if math.abs(got.x - want.x) > 1e-6 or math.abs(got.y - want.y) > 1e-6 then
            return true
        end
    end
    return false
end

function onTick()
    if not watchedWall or #dividers == 0 then
        syncDirty = false
        dirtyCause = nil
        return
    end
    local wall = acapi.getWall(watchedWall)
    if not wall then
        return
    end
    -- Undo/redo aftermath: adopt read-only, never write (any transaction
    -- wipes a pending redo). Guidance on divergence, then done.
    if syncDirty and (dirtyCause == "undo" or dirtyCause == "redo") then
        local cause = dirtyCause
        syncDirty = false
        dirtyCause = nil
        -- Refresh the key on adopt: otherwise it stays mismatched forever and
        -- a later tick falls through to a write-sync (the #16 mechanism).
        if readonlyAdopt("tick-" .. cause) then
            lastSync[watchedWall] = syncKey(wall)
        end
        guideIfDiverged(wall)
        if #dividers == 0 then
            ExecuteJS("stopTick();") -- adopted absence; place restarts the tick
        end
        return
    end
    -- Silent data time-travel (user-data undos/redos notify nothing): entry
    -- rev vs session rev is the only signal. Adopt read-only + silence the
    -- write path by refreshing lastSync (state is consistent by construction).
    if dirtyCause ~= "drag" then
        local entry = readEntry(watchedWall)
        local erev = (type(entry) == "table" and tonumber(entry.rev)) or nil
        if erev ~= lastRev then
            if readonlyAdopt("tick-rev") then
                lastSync[watchedWall] = syncKey(wall)
            end
            if #dividers == 0 then
                ExecuteJS("stopTick();")
            end
            return
        end
    end
    if not syncDirty then
        if lastSync[watchedWall] == syncKey(wall) then
            return -- in sync, stay silent (no log spam)
        end
        if guideIfDiverged(wall) then
            return
        end
    end
    handleWallSync(watchedWall, "tick")
    if #dividers == 0 then
        ExecuteJS("stopTick();") -- row fully pruned; place restarts the tick
    end
end

-- Replace the live row with a fresh division at the current opts.count.
-- Used by fresh Place and by Divisions-field changes (the field drives the
-- row, not just the UI): clears previous panels, places, watches, persists.
-- Reports via SetWebResult/logEvent; returns true on success.
local function replaceRow(wallGuid, partName)
    ROW_N = opts.count
    markStale()
    if liveCount() > 0 then
        -- Re-place replaces: clear the previous row first, else it orphans.
        -- Abort on flush failure so we never stack a new row on a live old one.
        -- Missing (in-flight) entries are skipped, never sent to delete.
        unwatchRow()
        acapi.beginUndo("Clear old dividers")
        for _, d in ipairs(dividers) do
            if not d.missing and not d.dead then
                acapi.delete(d.guid)
            end
        end
        local cok, cerr = acapi.endUndo()
        if not cok then
            SetWebResult("clear failed, place aborted: " .. tostring(cerr))
            return false
        end
        logEvent("cleared previous row")
        dividers = {}
    end
    local ok, info = placeDividers(wallGuid, partName)
    if not ok then
        SetWebResult("place failed: " .. tostring(info))
        return false
    end
    local wok, werr = acapi.watch(wallGuid, WATCHFUNC, { count = ROW_N })
    if wok then
        watchPanels()
        lastFailMsg = nil -- fresh row, old failure texts must show again if they recur
        syncDirty = false -- fresh place supersedes any deferred tick work
        dirtyCause = nil
        acapi.regWrite(REG_SEC, "partName", partName) -- persist only what placed
        ExecuteJS("startTick();")
        acapi.regWrite(REG_SEC, "wall", wallGuid) -- wipe hint for next run
        acapi.regWrite(REG_SEC, "watched", "true")
        local w = acapi.getWall(wallGuid)
        if w then lastSync[wallGuid] = syncKey(w) end
        local msg = string.format("Placed %d × '%s', watching wall", #dividers, tostring(info))
        if tostring(info):find(", 0 overrides") then
            msg = msg .. " — WARNING: no param override matched! Hit Inspect Part."
        end
        ExecuteJS("document.getElementById('result').textContent='" .. msg:gsub("'", "") .. "';")
        SetWebResult(msg)
        logEvent("placed " .. #dividers .. " points")
    else
        SetWebResult("watch failed: " .. tostring(werr))
        return false
    end
    return true
end

RegisterWebEvent("onPlaceDividers", function(args)
    disarmInhibit()
    applyFormArgs(args)
    local partName = args and args.partName or ""
    local guid = PickWall()
    if not guid then
        SetWebResult("Pick cancelled")
        return
    end
    watchedWall = guid
    -- Adopt an existing ledger entry when it matches (same part, or no part
    -- typed): no duplicate placement. A different typed part forces replace.
    do
        local row = readEntry(guid)
        if row then
            local wantInd = nil
            if partName ~= "" then
                local wi, we = resolvePart(partName)
                if wi == nil then
                    SetWebResult("part resolve failed: " .. tostring(we))
                    return
                end
                wantInd = wi
            end
            local aok, aerr = adoptRow(guid, row)
            if not aok then
                SetWebResult("adopt failed: " .. tostring(aerr))
                return
            end
            markStale()
            if #dividers > 0 and liveCount() == 0 then
                -- Corpse entry: never adopt (a Place yielding nothing is worse
                -- than burning the redo bridge for these guids — a later Redo
                -- resurrects them as unlinked duplicates, owned by the
                -- duplicate-prune item). Fall through to fresh placement.
                clearEntry(guid)
                logEvent("previous row discarded (all unreachable), placing fresh")
                dividers = {}
            elseif wantInd == nil or wantInd == rowMeta.libInd then
                local wok2, werr2 = acapi.watch(guid, WATCHFUNC, { count = ROW_N })
                if wok2 then
                    watchPanels()
                    ExecuteJS("startTick();")
                    acapi.regWrite(REG_SEC, "wall", guid) -- wipe hint for next run
                    acapi.regWrite(REG_SEC, "watched", "true")
                    local w2 = acapi.getWall(guid)
                    if w2 then lastSync[guid] = syncKey(w2) end
                    syncDirty = false -- adopted row is current; no deferred work pending
                    dirtyCause = nil
                    SetWebResult(string.format("Adopted %d dividers from wall data, watching", #dividers))
                    logEvent("adopted row from user data")
                else
                    SetWebResult("watch failed: " .. tostring(werr2))
                end
                return
            end
        end
    end
    replaceRow(guid, partName)
end)

RegisterWebEvent("onInspectPart", function(args)
    local partName = args and args.partName or ""
    local libInd, found = acapi.findObject(partName)
    if not libInd then
        SetWebResult("find failed: " .. tostring(found))
        return
    end
    local pars, perr = acapi.listParams(libInd)
    if not pars then
        SetWebResult("listParams failed: " .. tostring(perr))
        return
    end
    local hits = {}
    for _, p in ipairs(pars) do
        if p.name == "A" or p.name == "B" or p.name == "ZZYZX" then
            table.insert(hits, string.format("%s t=%d m=%d v=%s",
                p.name, p.typeID, p.typeMod, tostring(p.value)))
        end
    end
    -- NOTE: SetWebResult truncates at ~500 chars; keep it short
    local info = acapi.libInfo(partName)
    local flags = "?"
    if info then
        flags = string.format("idx=%d type=%d tmpl=%s place=%s",
            info.index, info.libType,
            info.isTemplate and "Y" or "n", info.isPlaceable and "Y" or "n")
    end
    SetWebResult(string.format("'%s' [%s]: %d params; A/B/ZZYZX: %s",
        tostring(found), flags, #pars, table.concat(hits, " | ")))
end)

RegisterWebEvent("onTick", function()
    onTick()
end)

RegisterWebEvent("onCountChanged", function(args)
    local n = args and clampCount(args.count) or nil
    if n == nil then
        SetWebResult("Division count must be an integer 1..100")
        return
    end
    opts.count = n
    saveOpts()
    if not watchedWall or #dividers == 0 then
        SetWebResult(string.format("Division count set to %d (applies on next Place)", n))
        return
    end
    if n == ROW_N then
        SetWebResult(string.format("Already %d divisions", n))
        return
    end
    -- The field drives the live row: re-divide in place (same wall, same part).
    local partName = rowMeta.partName or ""
    if partName == "" then
        SetWebResult("Row has no part recorded — re-place manually")
        return
    end
    disarmInhibit()
    replaceRow(watchedWall, partName) -- reports itself
end)

RegisterWebEvent("onEnlistDividers", function()
    -- Dumps the tracked table (slot, guid, state) to the event log panel.
    if #dividers == 0 then
        SetWebResult("No dividers tracked")
        return
    end
    local live, n = 0, #dividers
    for i, d in ipairs(dividers) do
        local state = "?"
        if type(d) ~= "table" or not d.guid then
            state = "broken-entry"
        elseif d.dead then
            state = "dead"
        elseif d.missing then
            state = "missing"
        elseif acapi.get(d.guid) then
            state = "live"
            live = live + 1
        else
            state = "unreachable-now"
        end
        logEvent(string.format("slot %d: %s [%s]", i, tostring(d.guid or "?"):sub(1, 8), state))
    end
    SetWebResult(string.format("Enlisted %d slots (%d live) — see event log", n, live))
end)

RegisterWebEvent("onRefreshDividers", function(args)
    applyFormArgs(args)
    if not watchedWall then
        SetWebResult("Pick a wall first!")
        return
    end
    disarmInhibit() -- explicit Refresh (doubles as Repair)
    -- Repair (explicit Refresh only, never the tick): an orphaned entry
    -- rebuilds from the ledger instead of failing. The tick must not do
    -- this — auto-repair would resurrect deliberate undos and duplicate
    -- on redo-tennis.
    markStale()
    local entry = readEntry(watchedWall)
    if entry ~= nil and #entryGuids(entry) > 0 and liveCount() == 0 then
        if #dividers == 0 then
            local aok, aerr = adoptRow(watchedWall, entry, true)
            if not aok then
                SetWebResult("Repair failed: " .. tostring(aerr))
                return
            end
            markStale()
        end
        local ok, info = repairRow(watchedWall, entry)
        if ok then
            syncDirty = false -- manual repair resolved everything pending
            dirtyCause = nil
            local msg = string.format("Repaired %d points from wall data", #dividers)
            SetWebResult(msg)
            logEvent(msg)
        else
            SetWebResult("Repair failed: " .. tostring(info))
        end
        return
    end
    local ok, info = refreshDividers(watchedWall)
    if ok then
        syncDirty = false -- manual refresh resolved everything pending
        dirtyCause = nil
        local msg = string.format("Refreshed %d points manually", #dividers)
        SetWebResult(msg)
        logEvent(msg)
    else
        SetWebResult("Refresh failed: " .. tostring(info))
    end
end)

RegisterWebEvent("onDeleteDividers", function()
    disarmInhibit()
    markStale()
    if liveCount() == 0 then
        -- No live panels: drop the orphaned entry (if any) instead of
        -- refusing — otherwise a zero-live row strands Place (adopts a
        -- corpse) while Delete refuses (nothing live to delete).
        local entry = watchedWall and readEntry(watchedWall) or nil
        if entry ~= nil or #dividers > 0 then
            unwatchRow()
            dividers = {}
            lastRev = nil
            dirtyCause = nil
            if watchedWall then
                clearEntry(watchedWall)
            end
            acapi.regWrite(REG_SEC, "watched", "false")
            ExecuteJS("stopTick();")
            SetWebResult("No live dividers — orphaned row cleared, place again to start fresh")
            logEvent("orphaned row cleared")
        else
            SetWebResult("No dividers to delete")
        end
        return
    end
    unwatchRow()
    acapi.beginUndo("Delete dividers")
    local n = 0
    for _, d in ipairs(dividers) do
        if not d.missing and not d.dead and acapi.delete(d.guid) then
            n = n + 1
        end
    end
    local eok, eerr = acapi.endUndo()
    if not eok then
        SetWebResult("delete flush failed: " .. tostring(eerr))
        return
    end
    dividers = {}
    lastRev = nil
    dirtyCause = nil
    if watchedWall then
        clearEntry(watchedWall)
    end
    acapi.regWrite(REG_SEC, "watched", "false")
    ExecuteJS("stopTick();")
    SetWebResult(string.format("Deleted %d dividers", n))
end)

RegisterWebEvent("onUnwatchDividers", function()
    if watchedWall then
        unwatchRow()
        disarmInhibit()
        clearEntry(watchedWall) -- release: panels stay in the model as plain
                                -- elements; the script forgets them for good
        dividers = {}
        lastRev = nil
        dirtyCause = nil
        syncDirty = false
        ExecuteJS("stopTick();")
        acapi.regWrite(REG_SEC, "watched", "false")
        SetWebResult("Unwatched — panels released (kept in model, no longer managed)")
        watchedWall = nil
        lastSync = {}
    else
        SetWebResult("Nothing watched")
    end
end)

ShowWebDialog([[
<html><head><style>
body{background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:16px;margin:0;}
button{background:#0e639c;color:#fff;border:none;padding:8px 16px;font-size:14px;cursor:pointer;border-radius:3px;margin:0 8px 8px 0;}
label{display:inline-block;width:130px;font-size:12px;}
input{margin:4px 0;width:180px;}
#result{margin-top:12px;padding:8px;background:#2d2d2d;border-radius:3px;font-size:13px;}
#events{margin-top:8px;padding:8px;background:#252525;border-radius:3px;font-size:11px;font-family:Consolas,monospace;max-height:120px;overflow-y:auto;white-space:pre-wrap;}
.row{margin:4px 0;}
</style></head><body>
<div class='row'><label>Marker part name:</label><input id='partName' type='text' value='' placeholder='exact library document name'></div>
<div class='row'><label>Divisions:</label><input id='divCount' type='number' value='10' min='1' max='100' step='1' onchange='countChanged()' title='re-divides the live row immediately'></div>
<div class='row'><label>Panel B (width):</label><input id='panelB' type='text' value='' placeholder='auto = wall width' oninput='fieldEdited()'></div>
<div class='row'><label>ZZYZX:</label><input id='panelZZ' type='number' value='0.25' step='0.05' oninput='fieldEdited()'></div>
<div class='row'><label>Center on point:</label><input id='centerDiv' type='checkbox' onchange='refreshNow()' title='panel midpoint (not origin) sits on the division; applies immediately'></div>
<div>
<button onclick='placeDividers()'>Pick Wall + Place Points</button>
<button onclick='inspectPart()'>Inspect Part</button>
<button onclick='refreshNow()'>Refresh Now</button>
<button onclick="archilua.DispatchEvent('onEnlistDividers')">Enlist Dividers</button>
</div>
<div>
<button onclick="archilua.DispatchEvent('onDeleteDividers')">Delete Points</button>
<button onclick="archilua.DispatchEvent('onUnwatchDividers')">Unwatch</button>
</div>
<div id='result'>Enter a marker part name, then pick a wall.</div>
<div id='events'>event log…</div>
<div id='ver' style='margin-top:8px;font-size:10px;color:#777;'>try_dividers.lua ]] .. SCRIPT_VER .. [[</div>
<script>
var eventLines = [];
function eventLog(line){
    eventLines.unshift(new Date().toLocaleTimeString() + ' ' + line);
    if(eventLines.length > 10) eventLines.pop();
    document.getElementById('events').textContent = eventLines.join('\n');
}
function formArgs(){
    var f = function(id){ return document.getElementById(id); };
    return {
        partName: f('partName').value,
        b: f('panelB').value,
        zzyzx: f('panelZZ').value,
        center: f('centerDiv').checked
    };
}
function placeDividers(){
    document.getElementById('result').textContent = 'Click a wall in the ArchiCAD viewport...';
    archilua.CallLua('onPlaceDividers', JSON.stringify(formArgs()));
}
var fieldTimer = null;
function fieldEdited(){
    if(fieldTimer) clearTimeout(fieldTimer);
    fieldTimer = setTimeout(function(){ fieldTimer = null; refreshNow(); }, 700);
}
function refreshNow(){
    archilua.CallLua('onRefreshDividers', JSON.stringify(formArgs()));
}
function inspectPart(){
    var f = function(id){ return document.getElementById(id); };
    archilua.CallLua('onInspectPart', JSON.stringify({ partName: f('partName').value }));
}
function countChanged(){
    archilua.CallLua('onCountChanged', JSON.stringify({ count: parseInt(document.getElementById('divCount').value, 10) }));
}
var tickTimer = null;
function startTick(){
    if(tickTimer) return;
    tickTimer = setInterval(function(){ archilua.DispatchEvent('onTick'); }, 800);
}
function stopTick(){
    if(tickTimer){ clearInterval(tickTimer); tickTimer = null; }
}
</script></body></html>
]])

-- Restore persisted options + form values + previous session state
do
    loadOpts()
    local saved = acapi.regRead(REG_SEC, "partName", "")
    if type(saved) == "string" and saved ~= "" then
        ExecuteJS("document.getElementById('partName').value='" .. saved:gsub("'", "") .. "';")
    end
    if opts.b ~= nil then
        ExecuteJS(string.format("document.getElementById('panelB').value='%s';", tostring(opts.b)))
    end
    ExecuteJS(string.format("document.getElementById('panelZZ').value='%s';", tostring(opts.zzyzx)))
    ExecuteJS(string.format("document.getElementById('centerDiv').checked=%s;",
        opts.center and "true" or "false"))
    ExecuteJS(string.format("document.getElementById('divCount').value='%d';", opts.count))
    acapi.observerLog(true) -- event tracing always on (see Report window)
    -- (footer version is baked into the HTML above: load-time ExecuteJS races the DOM)
    -- Ownership wipe: the previous run's children are deleted and the own
    -- entry cleared — every Run starts empty. Foreign entries survive.
    -- Registry identity keys (dividers/angles/dead/libInd/rowCount) are retired.
    local w = acapi.regRead(REG_SEC, "wall", "")
    if type(w) == "string" and w ~= "" and acapi.getWall(w) then
        local entry = readEntry(w)
        if entry ~= nil then
            local owned = entryGuids(entry)
            local wiped = 0
            if #owned > 0 then
                acapi.beginUndo("Clear previous run")
                for _, g in ipairs(owned) do
                    if acapi.get(g) and acapi.delete(g) then
                        wiped = wiped + 1
                    end
                end
                local cok, cerr = acapi.endUndo()
                if not cok then
                    logEvent("wipe failed: " .. tostring(cerr))
                end
            end
            clearEntry(w)
            if wiped > 0 then
                logEvent(string.format("wiped previous run: %d objects deleted", wiped))
            else
                logEvent("previous entry cleared (nothing live to delete)")
            end
        end
    end
    -- Last log call = top line of the event panel: always shows what's running.
    logEvent("try_dividers.lua " .. SCRIPT_VER .. " loaded")
end
