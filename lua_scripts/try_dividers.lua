-- try_dividers.lua — 10 standalone divider points on a wall, kept in sync via the observer.
-- Dividers are independent library objects on the wall centerline (visible 2D plan points).
-- On wall Change/Edit their XY positions are recomputed from the new begC/endC.

-- Bump on every script change; shown in the dialog footer to verify what's running.
local SCRIPT_VER = "2026-09-16t"

local watchedWall = nil
local dividers = {} -- divider entries {guid, angle}, in wall order
local DIV_COUNT_DEFAULT = 10
local REG_SEC = "try_dividers"
-- Live row's count (set at place/restore). The UI setting (opts.count) only
-- takes effect on next Place, so a live row's slot mapping never shifts.
local ROW_N = DIV_COUNT_DEFAULT
local WATCHFUNC = "try_dividers.lua\\onDividersWallEvent"

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

-- Forward declaration: pruneStale (below) runs before the definition.
local logEvent

-- Script globals die on every re-run while the C++ watch survives, so the
-- wall guid, divider guids/angles, libInd and watch flag persist in registry.
local function persistState()
    acapi.regWrite(REG_SEC, "wall", watchedWall or "")
    acapi.regWrite(REG_SEC, "watched", watchedWall ~= nil and "true" or "false")
    local gs, as, ds = {}, {}, {}
    for i, d in ipairs(dividers) do
        gs[i] = (type(d) == "table" and d.guid) or ""
        as[i] = (type(d) == "table" and d.angle) and string.format("%.6f", d.angle) or "0"
        ds[i] = (type(d) == "table" and d.dead) and "1" or "0"
    end
    acapi.regWrite(REG_SEC, "dividers", table.concat(gs, ","))
    acapi.regWrite(REG_SEC, "angles", table.concat(as, ","))
    acapi.regWrite(REG_SEC, "dead", table.concat(ds, ","))
    acapi.regWrite(REG_SEC, "libInd", dividers.libInd ~= nil and tostring(dividers.libInd) or "")
    acapi.regWrite(REG_SEC, "rowCount", tostring(ROW_N))
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
    return missing
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

local function splitCsv(s)
    local out = {}
    if type(s) ~= "string" or s == "" then return out end
    for part in s:gmatch("([^,]+)") do
        table.insert(out, part)
    end
    return out
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

local function resolvePart(name)
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
    acapi.regWrite(REG_SEC, "partName", partName)
    local wallLen = math.sqrt((wall.endC.x - wall.begC.x) ^ 2 + (wall.endC.y - wall.begC.y) ^ 2)
    local spacing = wallLen / ROW_N -- panel X size tiles the wall exactly
    local params = { A = spacing, B = effB(wall), ZZYZX = opts.zzyzx }
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
    -- stash for rebuilds on rotation change
    dividers.libInd = libInd
    persistState()
    return true, foundName .. " (" .. tostring(info) .. ")"
end

local function refreshDividers(wallGuid)
    local wall = acapi.getWall(wallGuid)
    if not wall then
        return false, "wall not found"
    end
    if #dividers == 0 then
        return false, "no dividers tracked (place first)"
    end
    markStale()
    if liveCount() == 0 then
        if #dividers == 0 then
            return false, "no dividers tracked (place first)"
        end
        return false, "all dividers unreachable (redo may restore them)"
    end
    local dx = wall.endC.x - wall.begC.x
    local dy = wall.endC.y - wall.begC.y
    local spacing = math.sqrt(dx * dx + dy * dy) / ROW_N
    local newAngle = wallAngle(wall)
    local params = { A = spacing, B = effB(wall), ZZYZX = opts.zzyzx }
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
        moves = moves, del = dels, libInd = dividers.libInd,
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
    persistState() -- guids may have changed on rotation rebuild
    return true
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

-- Last synced wall state per guid (geometry + opts). The drop fires both a
-- flushed edit AND a change; the second run would redo identical work as its
-- own undo step (generic set() has no change detection), so it is skipped.
local lastSync = {}

local function syncKey(wall)
    return string.format("%.6f,%.6f,%.6f,%.6f,%s,%s,%s",
        wall.begC.x, wall.begC.y, wall.endC.x, wall.endC.y,
        tostring(opts.b), tostring(opts.zzyzx), tostring(opts.center))
end

local lastFailMsg = nil

local function reportSync(kind, ok, info)
    if ok then
        syncDirty = false
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
    local ok, info = refreshDividers(guid)
    if ok and key ~= nil then
        lastSync[guid] = key
    end
    reportSync(kind, ok, info)
end

function onDividersWallEvent(guid, kwargs, kind)
    if kind == "undo" or kind == "redo" then
        -- During undo/redo notifications NO ArchiCAD calls that write may run
        -- (not even getWall is touched here) — just flag; the heartbeat or the
        -- next event performs the actual re-sync from safe context.
        syncDirty = true
        logEvent("undo/redo seen (" .. tostring(kind) .. "), will re-sync")
        return
    end
    if kind == "delete" then
        -- True deletion in plan (undo tennis arrives as undo/redo instead).
        if guid == watchedWall then
            unwatchRow()
            watchedWall = nil
            lastSync = {}
            ExecuteJS("stopTick();")
            persistState()
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
    handleWallSync(guid, kind)
end

-- Called by the JS heartbeat (800 ms while watching). Same safe context as a
-- button click, so commands are allowed here. Besides retrying refused syncs,
-- every beat compares the wall hash: undo/redo (which we must not write
-- during, and which split wall and panels into separate undo units) shows up
-- as drift and gets re-synced here — eventual consistency without polling writes.
-- A reverted sync (panels moved back, wall unchanged) is GUIDED, never
-- rewritten: rewriting it would trap the user (wall-undo unreachable behind
-- an auto-redoing sync), so we only explain the next Ctrl+Z.
local function panelsDiverged(wall)
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
        return
    end
    if not syncDirty then
        local wall = acapi.getWall(watchedWall)
        if not wall then
            return
        end
        if lastSync[watchedWall] == syncKey(wall) then
            return -- in sync, stay silent (no log spam)
        end
        if panelsDiverged(wall) then
            if os.time() - lastGuideTime >= 10 then
                lastGuideTime = os.time()
                local msg = "Divider sync was undone — Ctrl+Z again undoes the wall move"
                SetWebResult(msg)
                logEvent(msg)
            end
            return
        end
    end
    handleWallSync(watchedWall, "tick")
    if #dividers == 0 then
        ExecuteJS("stopTick();") -- row fully pruned; place restarts the tick
    end
end

RegisterWebEvent("onPlaceDividers", function(args)
    applyFormArgs(args)
    local partName = args and args.partName or ""
    local guid = PickWall()
    if not guid then
        SetWebResult("Pick cancelled")
        return
    end
    watchedWall = guid
    ROW_N = opts.count -- the setting takes effect on Place; refresh keeps ROW_N
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
            return
        end
        logEvent("cleared previous row")
        dividers = {}
    end
    local ok, info = placeDividers(guid, partName)
    if not ok then
        SetWebResult("place failed: " .. tostring(info))
        return
    end
    local wok, werr = acapi.watch(guid, WATCHFUNC, { count = ROW_N })
    if wok then
        for _, d in ipairs(dividers) do
            acapi.watch(d.guid, WATCHFUNC, { panel = true })
        end
        lastFailMsg = nil -- fresh row, old failure texts must show again if they recur
        ExecuteJS("startTick();")
        local w = acapi.getWall(guid)
        if w then lastSync[guid] = syncKey(w) end
        local msg = string.format("Placed %d × '%s', watching wall", #dividers, tostring(info))
        if tostring(info):find(", 0 overrides") then
            msg = msg .. " — WARNING: no param override matched! Hit Inspect Part."
        end
        ExecuteJS("document.getElementById('result').textContent='" .. msg:gsub("'", "") .. "';")
        SetWebResult(msg)
        logEvent("placed " .. #dividers .. " points")
    else
        SetWebResult("watch failed: " .. tostring(werr))
    end
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

RegisterWebEvent("onToggleLog", function(args)
    local on = args and args.enabled
    acapi.observerLog(on)
    SetWebResult("event logging " .. (on and "ON (see Report window)" or "OFF"))
end)

RegisterWebEvent("onToggleAuto", function(args)
    -- Persisted silently; restored on load. No result spam on mere toggle.
    acapi.regWrite(REG_SEC, "autoRef", (args and args.enabled) and "true" or "false")
end)

RegisterWebEvent("onCountChanged", function(args)
    local n = args and clampCount(args.count) or nil
    if n == nil then
        SetWebResult("Division count must be an integer 1..100")
        return
    end
    opts.count = n
    saveOpts()
    if #dividers > 0 and ROW_N ~= n then
        SetWebResult(string.format("Division count set to %d — re-place to apply (current row keeps %d)", n, ROW_N))
    else
        SetWebResult(string.format("Division count set to %d", n))
    end
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
    local ok, info = refreshDividers(watchedWall)
    if ok then
        local msg = string.format("Refreshed %d points manually", #dividers)
        SetWebResult(msg)
        logEvent(msg)
    else
        SetWebResult("Refresh failed: " .. tostring(info))
    end
end)

RegisterWebEvent("onDeleteDividers", function()
    markStale()
    if liveCount() == 0 then
        SetWebResult("No dividers to delete")
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
    persistState()
    ExecuteJS("stopTick();")
    SetWebResult(string.format("Deleted %d dividers", n))
end)

RegisterWebEvent("onUnwatchDividers", function()
    if watchedWall then
        unwatchRow()
        ExecuteJS("stopTick();")
        SetWebResult("Unwatched (points stay in the plan)")
        watchedWall = nil
        lastSync = {}
        persistState()
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
<div class='row'><label>Divisions:</label><input id='divCount' type='number' value='10' min='1' max='100' step='1' onchange='countChanged()' title='takes effect on next Place'></div>
<div class='row'><label>Panel B (width):</label><input id='panelB' type='text' value='' placeholder='auto = wall width' oninput='fieldEdited()'></div>
<div class='row'><label>ZZYZX:</label><input id='panelZZ' type='number' value='0.25' step='0.05' oninput='fieldEdited()'></div>
<div class='row'><label>Center on point:</label><input id='centerDiv' type='checkbox' onchange='refreshNow()' title='panel midpoint (not origin) sits on the division; applies immediately'></div>
<div class='row'><label>Auto-refresh:</label><input id='autoRef' type='checkbox' onchange='saveAutoRef()' title='apply B/ZZYZX edits immediately, no Refresh button needed'></div>
<div class='row'><label>Log events:</label><input id='logEvents' type='checkbox' onchange='toggleLog()' title='raw observer notifications to the Report window'></div>
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
<div id='ver' style='margin-top:8px;font-size:10px;color:#777;'></div>
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
    if(!document.getElementById('autoRef').checked) return;
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
function toggleLog(){
    archilua.CallLua('onToggleLog', JSON.stringify({ enabled: document.getElementById('logEvents').checked }));
}
function saveAutoRef(){
    archilua.CallLua('onToggleAuto', JSON.stringify({ enabled: document.getElementById('autoRef').checked }));
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
    ExecuteJS(string.format("document.getElementById('autoRef').checked=%s;",
        acapi.regRead(REG_SEC, "autoRef", "false") == "true" and "true" or "false"))
    ExecuteJS(string.format("document.getElementById('divCount').value='%d';", opts.count))
    ExecuteJS(string.format("document.getElementById('ver').textContent='try_dividers.lua %s';", SCRIPT_VER))
    -- Restore previous session: wall + dividers survive re-runs (the C++ watch
    -- does too), so refresh keeps working without re-placing.
    local w = acapi.regRead(REG_SEC, "wall", "")
    if type(w) == "string" and w ~= "" and acapi.getWall(w) then
        watchedWall = w
        local gs = splitCsv(acapi.regRead(REG_SEC, "dividers", ""))
        local as = splitCsv(acapi.regRead(REG_SEC, "angles", ""))
        local ds = splitCsv(acapi.regRead(REG_SEC, "dead", ""))
        dividers = {}
        for i, g in ipairs(gs) do
            dividers[i] = { guid = g, angle = tonumber(as[i]) or 0 }
            if ds[i] == "1" then
                dividers[i].dead = true
            end
        end
        dividers.libInd = tonumber(acapi.regRead(REG_SEC, "libInd", ""))
        ROW_N = clampCount(acapi.regRead(REG_SEC, "rowCount", "")) or #dividers
        if acapi.regRead(REG_SEC, "watched", "false") == "true" and #dividers > 0 then
            acapi.watch(w, WATCHFUNC, { count = ROW_N })
            for _, d in ipairs(dividers) do
                acapi.watch(d.guid, WATCHFUNC, { panel = true })
            end
            ExecuteJS("startTick();")
            logEvent("restored " .. #dividers .. " dividers, watching")
        elseif #dividers > 0 then
            logEvent("restored " .. #dividers .. " dividers (not watching)")
        end
    end
    -- Last log call = top line of the event panel: always shows what's running.
    logEvent("try_dividers.lua " .. SCRIPT_VER .. " loaded")
end
