-- try_undorow.lua — PoC: undoable element user data + read-only re-adopt on
-- undo/redo. No panels, no sync, no heartbeat: pure event-driven ledger.
--
-- Entry shape: userData["try_undorow.lua"] = {rev, note, wall={begC,endC}}.
-- Policy (mirrors the divider design in miniature):
--   edit/change (fresh gesture, no redo pending) → adopt + rewrite geometry;
--   undo/redo/delete                           → adopt READ-ONLY, never write.
-- Hypothesis under test: ArchiCAD time-travels the entry natively, so
-- read-only adopt keeps session and DB consistent without ever wiping redo.
--
-- Acceptance: Bump x3, then Z,Z,Z must keep Edit > Redo enabled after every
-- Z with adopted rev reading 2,1,absent; Y,Y,Y must read 1,2,3.

local SCRIPT_VER = "260922g"
local ROW_KEY = "try_undorow.lua"
local WATCHFUNC = "try_undorow.lua\\onUndoRowEvent"

local watchedWall = nil
local kindCount = 0
local geomStale = false -- entry geometry outdated vs live wall (flush on Bump)
local lastRev = nil -- entry revision mirror; tick detects silent walks by mismatch

-- Display-only: JSON round-trips integers as floats ("rev=2.0"); show whole
-- numbers without the .0, "none" for nil (storage and arithmetic untouched).
local function revStr(v)
    local n = tonumber(v)
    if n == nil then return "none" end
    local i = math.tointeger(n)
    if i ~= nil then return tostring(i) end
    return tostring(n)
end

local function geomHash(wall)
    if not wall or not wall.begC or not wall.endC then
        return "?"
    end
    return string.format("(%.3f,%.3f)-(%.3f,%.3f)",
        wall.begC.x, wall.begC.y, wall.endC.x, wall.endC.y)
end

local function entryGeomHash(entry)
    if not entry or not entry.wall then
        return "?"
    end
    return geomHash(entry.wall)
end

local function geomMatch(entry, wall)
    if not entry or not entry.wall or not wall or not wall.begC or not wall.endC then
        return false
    end
    local e = entry.wall
    local function close(a, b) return math.abs(a - b) < 1e-6 end
    return close(e.begC.x, wall.begC.x) and close(e.begC.y, wall.begC.y)
        and close(e.endC.x, wall.endC.x) and close(e.endC.y, wall.endC.y)
end

local function showState(text)
    ExecuteJS(string.format("document.getElementById('state').textContent='%s';",
        tostring(text):gsub("'", "")))
end

local function logKind(kind, detail)
    kindCount = kindCount + 1
    ExecuteJS(string.format("if (typeof eventLog === 'function') { eventLog('#%d %s %s'); }",
        kindCount, tostring(kind):gsub("'", ""), tostring(detail or ""):gsub("'", "")))
end

-- Own entry fetch (nil when the wall carries none — a valid stack position).
local function readEntry(wallGuid)
    local ud = acapi.getUserData(wallGuid)
    if type(ud) == "table" and type(ud[ROW_KEY]) == "table" then
        return ud[ROW_KEY]
    end
    return nil
end

-- Read-modify-write of OWN entry only (foreign keys preserved).
local function writeEntry(wallGuid, rev, wall)
    local ud = acapi.getUserData(wallGuid)
    if type(ud) ~= "table" then
        ud = {}
    end
    local entry = { rev = rev, note = "undorow" }
    if wall and wall.begC and wall.endC then
        entry.wall = { begC = { x = wall.begC.x, y = wall.begC.y },
                       endC = { x = wall.endC.x, y = wall.endC.y } }
    end
    ud[ROW_KEY] = entry
    local ok, err = acapi.setUserData(wallGuid, ud)
    if ok then
        lastRev = rev
        logKind("write", "button rev=" .. revStr(rev))
    else
        logKind("write", "button FAILED " .. tostring(err))
    end
    return ok
end

-- Re-adopt the (natively versioned) entry and refresh the rev printout.
-- Strictly read-only on every path: v2 performs zero notification-context
-- writes by construction, so undo/redo refreshes the UI exactly like the
-- kind log does — same ExecuteJS mechanism — without touching redo.
local function adoptAndShow(tag)
    if not watchedWall then
        return nil
    end
    local wall = acapi.getWall(watchedWall)
    if not wall then
        showState("[" .. tostring(tag) .. "] wall unreadable")
        return nil
    end
    local entry = readEntry(watchedWall)
    lastRev = (entry and tonumber(entry.rev)) or nil -- session tracks revision
    if entry == nil then
        showState(string.format("[%s] no entry (pre-write stack position) | live %s%s",
            tostring(tag), geomHash(wall), geomStale and " | GEOM-STALE" or ""))
    else
        showState(string.format("[%s] rev=%s | stored %s | live %s %s%s",
            tostring(tag), revStr(entry.rev), entryGeomHash(entry), geomHash(wall),
            (geomMatch(entry, wall) and "MATCH" or "DIVERGED"),
            geomStale and " | GEOM-STALE" or ""))
    end
    return entry
end

function onUndoRowEvent(guid, kwargs, kind)
    if guid ~= watchedWall then
        logKind(kind, "ignored (not watched)")
        return
    end
    if kind == "delete" then
        watchedWall = nil
        geomStale = false
        showState("wall deleted — pick again")
        logKind(kind, "released")
        return
    end
    if kind == "undo" or kind == "redo" then
        local entry = adoptAndShow(kind) -- read-only rev refresh, redo untouched
        logKind(kind, "adopted read-only rev=" .. revStr(entry and entry.rev))
        return
    end
    if kind == "edit" or kind == "change" then
        geomStale = true -- geometry follows on next Bump (no writes in dispatch)
        local entry = adoptAndShow(kind)
        logKind(kind, "adopted read-only rev=" .. revStr(entry and entry.rev) .. ", geometry STALE")
        return
    end
end

RegisterWebEvent("onPickWall", function()
    local guid = PickWall()
    if not guid then
        SetWebResult("Pick cancelled")
        return
    end
    watchedWall = guid
    geomStale = false
    local wall = acapi.getWall(guid)
    writeEntry(guid, 1, wall)
    acapi.watch(guid, WATCHFUNC, { poc = 1 })
    adoptAndShow("pick")
    ExecuteJS("startTick();")
    SetWebResult("Watching wall, rev=1 written (own undo unit)")
end)

RegisterWebEvent("onBump", function()
    if not watchedWall then
        SetWebResult("Pick a wall first!")
        return
    end
    local entry = readEntry(watchedWall)
    local rev = (entry and tonumber(entry.rev) or 0) + 1
    local wall = acapi.getWall(watchedWall)
    writeEntry(watchedWall, rev, wall) -- button context: flushes pending geometry
    geomStale = false
    adoptAndShow("bump")
    SetWebResult("rev=" .. revStr(rev) .. " written (own undo unit, geometry flushed)")
end)

RegisterWebEvent("onReread", function()
    if not watchedWall then
        SetWebResult("Pick a wall first!")
        return
    end
    -- Button-context adopt: if this shows a stepped-back rev that the
    -- notification-time adopt missed, reads inside Undo notifications see
    -- pre-rollback state (a finding the whole undo design depends on).
    local entry = adoptAndShow("reread")
    logKind("reread", "button rev=" .. revStr(entry and entry.rev))
    SetWebResult("re-read rev=" .. revStr(entry and entry.rev) .. " (compare with notification-time readout)")
end)

RegisterWebEvent("onToggleLog", function(args)
    local on = args and args.enabled
    acapi.observerLog(on)
    SetWebResult("event logging " .. (on and "ON (see Report window)" or "OFF"))
end)

-- Polling heartbeat: the ONLY channel that sees silent user-data undos/redos
-- (ArchiCAD notifies nothing for them). Rev mismatch against the session
-- mirror adopts read-only + logs + refreshes the printout, never writes.
RegisterWebEvent("onTick", function()
    if not watchedWall then
        return
    end
    local entry = readEntry(watchedWall)
    local erev = (entry and tonumber(entry.rev)) or nil
    if erev ~= lastRev then
        adoptAndShow("tick-rev")
        logKind("tick-rev", "adopted read-only rev=" .. tostring(erev))
    end
end)

ShowWebDialog([[
<html><head><style>
body{background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:16px;margin:0;}
button{background:#0e639c;color:#fff;border:none;padding:8px 16px;font-size:14px;cursor:pointer;border-radius:3px;margin:0 8px 8px 0;}
#result{margin-top:12px;padding:8px;background:#2d2d2d;border-radius:3px;font-size:13px;}
#state{margin-top:8px;padding:8px;background:#2d2d2d;border-radius:3px;font-size:13px;font-family:Consolas,monospace;}
#events{margin-top:8px;padding:8px;background:#252525;border-radius:3px;font-size:11px;font-family:Consolas,monospace;max-height:140px;overflow-y:auto;white-space:pre-wrap;}
.row{margin:4px 0;}
label{display:inline-block;width:130px;font-size:12px;}
input{margin:4px 0;}
</style></head><body>
<div>
<button onclick="archilua.DispatchEvent('onPickWall')">Pick Wall (rev=1)</button>
<button onclick="archilua.DispatchEvent('onBump')">Bump rev</button>
<button onclick="archilua.DispatchEvent('onReread')">Re-read (button context)</button>
</div>
<div class='row'><label>Log events:</label><input id='logEvents' type='checkbox' onchange='toggleLog()' title='raw observer notifications to the Report window'></div>
<div id='result'>Pick a wall to write rev=1 and watch it.</div>
<div id='check' style='margin-top:8px;padding:8px;background:#2d2d2d;border-radius:3px;font-size:11px;'>Acceptance: Bump x3, then Z,Z,Z (Redo stays enabled, rev 2,1,absent), then Y,Y,Y (rev 1,2,3).</div>
<div id='state'>no wall</div>
<div id='events'>event log…</div>
<div id='ver' style='margin-top:8px;font-size:10px;color:#777;'>try_undorow.lua ]] .. SCRIPT_VER .. [[</div>
<script>
var eventLines = [];
function eventLog(line){
    eventLines.unshift(new Date().toLocaleTimeString() + ' ' + line);
    if(eventLines.length > 10) eventLines.pop();
    document.getElementById('events').textContent = eventLines.join('\n');
}
function toggleLog(){
    archilua.CallLua('onToggleLog', JSON.stringify({ enabled: document.getElementById('logEvents').checked }));
}
var tickTimer = null;
function startTick(){
    if(tickTimer) return;
    tickTimer = setInterval(function(){ archilua.DispatchEvent('onTick'); }, 800);
}
</script></body></html>
]])

-- (footer version is baked into the HTML above: load-time ExecuteJS races the DOM)
ExecuteJS("if (typeof eventLog === 'function') { eventLog('try_undorow.lua " .. SCRIPT_VER .. " loaded'); }")
