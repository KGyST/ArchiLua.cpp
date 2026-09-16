-- try_dividers.lua — 10 standalone divider points on a wall, kept in sync via the observer.
-- Dividers are independent library objects on the wall centerline (visible 2D plan points).
-- On wall Change/Edit their XY positions are recomputed from the new begC/endC.

local watchedWall = nil
local dividers = {} -- divider object guids, in wall order
local DIV_COUNT = 10
local REG_SEC = "try_dividers"

local function wallPoint(wall, t)
    return {
        x = wall.begC.x + (wall.endC.x - wall.begC.x) * t,
        y = wall.begC.y + (wall.endC.y - wall.begC.y) * t
    }
end

local function wallAngle(wall)
    return math.atan(wall.endC.y - wall.begC.y, wall.endC.x - wall.begC.x)
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
    local spacing = (wall.endC.x - wall.begC.x) ^ 2 + (wall.endC.y - wall.begC.y) ^ 2
    spacing = math.sqrt(spacing) / DIV_COUNT -- panel X size tiles the wall exactly
    local params = { A = spacing, B = wall.thickness, ZZYZX = 0.25 }
    local ang = wallAngle(wall)
    local positions = {}
    for i = 1, DIV_COUNT do
        local p = wallPoint(wall, (i - 0.5) / DIV_COUNT)
        p.angle = ang
        positions[i] = p
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
    acapi.beginUndo("Sync dividers")
    for i, d in ipairs(dividers) do
        local p = wallPoint(wall, (i - 0.5) / DIV_COUNT)
        if math.abs(newAngle - d.angle) > 1e-6 then
            -- angle is not Change-editable: delete + recreate aligned to the wall
            p.angle = newAngle
            acapi.delete(d.guid)
            local g, err = acapi.create(dividers.libInd, p,
                { A = spacing, B = wall.thickness, ZZYZX = 0.25 })
            if not g then
                acapi.endUndo()
                return false
            end
            d.guid = g
            d.angle = newAngle
        else
            local ok = acapi.set(d.guid, { pos = p })
            if not ok then
                acapi.endUndo()
                return false
            end
            -- Re-tile panel X size so the row keeps filling the (possibly stretched) wall
            acapi.setparams(d.guid, { A = spacing })
        end
    end
    acapi.endUndo()
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
    if refreshDividers(guid) then
        local msg = string.format("Dividers synced (%s): %d points", tostring(kind), #dividers)
        SetWebResult(msg)
        logEvent(msg)
    else
        SetWebResult("Dividers sync failed (" .. tostring(kind) .. ")")
        logEvent("SYNC FAILED (" .. tostring(kind) .. ")")
    end
end

RegisterWebEvent("onPlaceDividers", function(args)
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
        local msg = string.format("Placed %d x '%s', watching wall", #dividers, tostring(info))
        if tostring(info):find("0 overrides") then
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
    SetWebResult(string.format("'%s': %d params; A/B/ZZYZX: %s",
        tostring(found), #pars, table.concat(hits, " | ")))
end)

RegisterWebEvent("onRefreshDividers", function()
    if not watchedWall then
        SetWebResult("Pick a wall first!")
        return
    end
    if refreshDividers(watchedWall) then
        SetWebResult(string.format("Refreshed %d points manually", #dividers))
    else
        SetWebResult("Refresh failed")
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
    SetWebResult(string.format("Deleted %d dividers", n))
end)

RegisterWebEvent("onUnwatchDividers", function()
    if watchedWall then
        acapi.unwatch(watchedWall)
        SetWebResult("Unwatched (points stay in the plan)")
        watchedWall = nil
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
<div>
<button onclick='placeDividers()'>Pick Wall + Place Points</button>
<button onclick='inspectPart()'>Inspect Part</button>
<button onclick="archilua.DispatchEvent('onRefreshDividers')">Refresh Now</button>
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
    if(eventLines.length > 6) eventLines.pop();
    document.getElementById('events').textContent = eventLines.join('\n');
}
function placeDividers(){
    var f = function(id){ return document.getElementById(id); };
    f('result').textContent = 'Click a wall in the ArchiCAD viewport...';
    archilua.CallLua('onPlaceDividers', JSON.stringify({ partName: f('partName').value }));
}
function inspectPart(){
    var f = function(id){ return document.getElementById(id); };
    archilua.CallLua('onInspectPart', JSON.stringify({ partName: f('partName').value }));
}
</script></body></html>
]])

-- Restore persisted part name
do
    local saved = acapi.regRead(REG_SEC, "partName", "")
    if type(saved) == "string" and saved ~= "" then
        ExecuteJS("document.getElementById('partName').value='" .. saved:gsub("'", "") .. "';")
    end
end
