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
    local libInd, foundOrErr = resolvePart(partName)
    if not libInd then
        return false, foundOrErr
    end
    local wall = acapi.getWall(wallGuid)
    if not wall then
        return false, "getWall failed"
    end
    acapi.regWrite(REG_SEC, "partName", partName)
    acapi.beginUndo("Place dividers")
    dividers = {}
    for i = 1, DIV_COUNT do
        local p = wallPoint(wall, (i - 0.5) / DIV_COUNT)
        local g, err = acapi.create(libInd, p)
        if not g then
            acapi.endUndo()
            return false, err
        end
        table.insert(dividers, g)
    end
    acapi.endUndo()
    return true
end

local function refreshDividers(wallGuid)
    local wall = acapi.getWall(wallGuid)
    if not wall or #dividers == 0 then
        return false
    end
    acapi.beginUndo("Sync dividers")
    for i, g in ipairs(dividers) do
        local p = wallPoint(wall, (i - 0.5) / DIV_COUNT)
        local ok, err = acapi.set(g, { pos = p })
        if not ok then
            acapi.endUndo()
            return false
        end
    end
    acapi.endUndo()
    return true
end

function onDividersWallEvent(guid, kwargs, kind)
    if kind ~= "edit" and kind ~= "change" then
        return
    end
    if refreshDividers(guid) then
        SetWebResult(string.format("Dividers synced (%s): %d points", tostring(kind), #dividers))
    else
        SetWebResult("Dividers sync failed (" .. tostring(kind) .. ")")
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
    local ok, err = placeDividers(guid, partName)
    if not ok then
        SetWebResult("place failed: " .. tostring(err))
        return
    end
    local wok, werr = acapi.watch(guid, "try_dividers.lua\\onDividersWallEvent", { count = DIV_COUNT })
    if wok then
        ExecuteJS("document.getElementById('result').textContent='Watching wall with " .. DIV_COUNT .. " points — move it.';")
        SetWebResult(string.format("Placed %d points, watching wall", #dividers))
    else
        SetWebResult("watch failed: " .. tostring(werr))
    end
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
    for _, g in ipairs(dividers) do
        if acapi.delete(g) then
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
.row{margin:4px 0;}
</style></head><body>
<div class='row'><label>Marker part name:</label><input id='partName' type='text' value='' placeholder='exact library document name'></div>
<div>
<button onclick='placeDividers()'>Pick Wall + Place Points</button>
<button onclick="archilua.DispatchEvent('onRefreshDividers')">Refresh Now</button>
</div>
<div>
<button onclick="archilua.DispatchEvent('onDeleteDividers')">Delete Points</button>
<button onclick="archilua.DispatchEvent('onUnwatchDividers')">Unwatch</button>
</div>
<div id='result'>Enter a marker part name, then pick a wall.</div>
<script>
function placeDividers(){
    var f = function(id){ return document.getElementById(id); };
    f('result').textContent = 'Click a wall in the ArchiCAD viewport...';
    archilua.CallLua('onPlaceDividers', JSON.stringify({ partName: f('partName').value }));
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
