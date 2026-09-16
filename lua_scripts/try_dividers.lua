-- try_dividers.lua — 10 standalone divider points on a wall, kept in sync via the observer.
-- Dividers are independent library objects on the wall centerline (visible 2D plan points).
-- On wall Change/Edit their XY positions are recomputed from the new begC/endC.

local watchedWall = nil
local dividers = {} -- divider entries {guid, angle}, in wall order
local DIV_COUNT = 10
local REG_SEC = "try_dividers"

-- Panel options (persisted). b = nil means auto = wall thickness.
local opts = { b = nil, zzyzx = 0.25, center = false }

local function loadOpts()
    opts.b = tonumber(acapi.regRead(REG_SEC, "B")) -- nil stays nil = auto
    opts.zzyzx = tonumber(acapi.regRead(REG_SEC, "ZZYZX")) or 0.25
    opts.center = acapi.regRead(REG_SEC, "center", "false") == "true"
end

local function saveOpts()
    acapi.regWrite(REG_SEC, "B", opts.b ~= nil and tostring(opts.b) or "")
    acapi.regWrite(REG_SEC, "ZZYZX", tostring(opts.zzyzx))
    acapi.regWrite(REG_SEC, "center", opts.center and "true" or "false")
end

local function effB(wall)
    return opts.b or wall.thickness
end

-- Script globals die on every re-run while the C++ watch survives, so the
-- wall guid, divider guids/angles, libInd and watch flag persist in registry.
local function persistState()
    acapi.regWrite(REG_SEC, "wall", watchedWall or "")
    acapi.regWrite(REG_SEC, "watched", watchedWall ~= nil and "true" or "false")
    local gs, as = {}, {}
    for i, d in ipairs(dividers) do
        gs[i] = d.guid
        as[i] = string.format("%.6f", d.angle)
    end
    acapi.regWrite(REG_SEC, "dividers", table.concat(gs, ","))
    acapi.regWrite(REG_SEC, "angles", table.concat(as, ","))
    acapi.regWrite(REG_SEC, "libInd", dividers.libInd ~= nil and tostring(dividers.libInd) or "")
end

local function splitCsv(s)
    local out = {}
    if type(s) ~= "string" or s == "" then return out end
    for part in s:gmatch("([^,]+)") do
        table.insert(out, part)
    end
    return out
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
    local p = wallPoint(wall, (i - 0.5) / DIV_COUNT)
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
    local spacing = wallLen / DIV_COUNT -- panel X size tiles the wall exactly
    local params = { A = spacing, B = effB(wall), ZZYZX = opts.zzyzx }
    local ang = wallAngle(wall)
    local positions = {}
    for i = 1, DIV_COUNT do
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
    if not wall or #dividers == 0 then
        return false
    end
    local dx = wall.endC.x - wall.begC.x
    local dy = wall.endC.y - wall.begC.y
    local spacing = math.sqrt(dx * dx + dy * dy) / DIV_COUNT
    local newAngle = wallAngle(wall)
    local params = { A = spacing, B = effB(wall), ZZYZX = opts.zzyzx }
    -- Partition: moves apply in place, direction changes need a rebuild
    -- (angle is not Change-editable). All rebuilds go through ONE createMany,
    -- so even a full rotation re-sync costs 2 undo steps, not 20.
    local rebuildIdx, rebuildPos = {}, {}
    acapi.beginUndo("Sync dividers")
    for i, d in ipairs(dividers) do
        if math.abs(newAngle - d.angle) > 1e-6 then
            table.insert(rebuildIdx, i)
            rebuildPos[#rebuildPos + 1] = divPoint(wall, spacing, newAngle, i)
        else
            local p = divPoint(wall, spacing, newAngle, i)
            local ok, err = acapi.set(d.guid, { pos = p })
            if not ok then
                acapi.endUndo()
                return false, "set " .. i .. ": " .. tostring(err)
            end
            -- Re-apply sizes so the row keeps filling the (possibly stretched) wall
            -- and follows edited B/ZZYZX values (no-ops when unchanged)
            local pok, perr = acapi.setParams(d.guid, params)
            if not pok then
                acapi.endUndo()
                return false, "setparams " .. i .. ": " .. tostring(perr)
            end
        end
    end
    for _, i in ipairs(rebuildIdx) do
        acapi.delete(dividers[i].guid)
    end
    -- endUndo is where the batch really executes: a refused nested command
    -- surfaces here (empty-message failures), never silently.
    local eok, eerr = acapi.endUndo()
    if not eok then
        return false, "endUndo flush refused: " .. tostring(eerr)
    end
    if #rebuildIdx > 0 then
        local guids, err = acapi.createMany(dividers.libInd, rebuildPos, params)
        if not guids then
            return false, "recreate: " .. tostring(err)
        end
        for k, i in ipairs(rebuildIdx) do
            dividers[i] = { guid = guids[k], angle = newAngle }
        end
    end
    persistState() -- guids may have changed on rotation rebuild
    return true
end

local eventLogCount = 0

local function logEvent(text)
    eventLogCount = eventLogCount + 1
    -- eventLog() is defined in the dialog HTML; falls back to result div
    ExecuteJS(string.format("if (typeof eventLog === 'function') { eventLog('#%d %s'); }",
        eventLogCount, text:gsub("'", "")))
end

function onDividersWallEvent(guid, kwargs, kind)
    if kind ~= "edit" and kind ~= "change" then
        return
    end
    local ok, info = refreshDividers(guid)
    if ok then
        local msg = string.format("Dividers synced (%s): %d points", tostring(kind), #dividers)
        SetWebResult(msg)
        logEvent(msg)
    else
        local msg = "Dividers sync failed (" .. tostring(kind) .. "): " .. tostring(info)
        SetWebResult(msg)
        logEvent(msg)
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
    local ok, info = placeDividers(guid, partName)
    if not ok then
        SetWebResult("place failed: " .. tostring(info))
        return
    end
    local wok, werr = acapi.watch(guid, "try_dividers.lua\\onDividersWallEvent", { count = DIV_COUNT })
    if wok then
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

RegisterWebEvent("onToggleLog", function(args)
    local on = args and args.enabled
    acapi.observerLog(on)
    SetWebResult("event logging " .. (on and "ON (see Report window)" or "OFF"))
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
    if #dividers == 0 then
        SetWebResult("No dividers to delete")
        return
    end
    acapi.beginUndo("Delete dividers")
    local n = 0
    for _, d in ipairs(dividers) do
        if acapi.delete(d.guid) then
            n = n + 1
        end
    end
    acapi.endUndo()
    dividers = {}
    persistState()
    SetWebResult(string.format("Deleted %d dividers", n))
end)

RegisterWebEvent("onUnwatchDividers", function()
    if watchedWall then
        acapi.unwatch(watchedWall)
        SetWebResult("Unwatched (points stay in the plan)")
        watchedWall = nil
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
<div class='row'><label>Panel B (width):</label><input id='panelB' type='text' value='' placeholder='auto = wall width' oninput='fieldEdited()'></div>
<div class='row'><label>ZZYZX:</label><input id='panelZZ' type='number' value='0.25' step='0.05' oninput='fieldEdited()'></div>
<div class='row'><label>Center on point:</label><input id='centerDiv' type='checkbox' onchange='fieldEdited()' title='panel midpoint (not origin) sits on the division'></div>
<div class='row'><label>Auto-refresh:</label><input id='autoRef' type='checkbox' title='apply UI edits immediately, no Refresh button needed'></div>
<div class='row'><label>Log events:</label><input id='logEvents' type='checkbox' onchange='toggleLog()' title='raw observer notifications to the Report window'></div>
<div>
<button onclick='placeDividers()'>Pick Wall + Place Points</button>
<button onclick='inspectPart()'>Inspect Part</button>
<button onclick='refreshNow()'>Refresh Now</button>
</div>
<div>
<button onclick="archilua.DispatchEvent('onDeleteDividers')">Delete Points</button>
<button onclick="archilua.DispatchEvent('onUnwatchDividers')">Unwatch</button>
</div>
<div id='result'>Enter a marker part name, then pick a wall.</div>
<div id='events'>event log…</div>
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
    -- Restore previous session: wall + dividers survive re-runs (the C++ watch
    -- does too), so refresh keeps working without re-placing.
    local w = acapi.regRead(REG_SEC, "wall", "")
    if type(w) == "string" and w ~= "" and acapi.getWall(w) then
        watchedWall = w
        local gs = splitCsv(acapi.regRead(REG_SEC, "dividers", ""))
        local as = splitCsv(acapi.regRead(REG_SEC, "angles", ""))
        dividers = {}
        for i, g in ipairs(gs) do
            dividers[i] = { guid = g, angle = tonumber(as[i]) or 0 }
        end
        dividers.libInd = tonumber(acapi.regRead(REG_SEC, "libInd", ""))
        if acapi.regRead(REG_SEC, "watched", "false") == "true" and #dividers > 0 then
            acapi.watch(w, "try_dividers.lua\\onDividersWallEvent", { count = DIV_COUNT })
            logEvent("restored " .. #dividers .. " dividers, watching")
        elseif #dividers > 0 then
            logEvent("restored " .. #dividers .. " dividers (not watching)")
        end
    end
end
