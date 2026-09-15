-- try_dividers.lua — 10 divider ticks on a wall, kept in sync via the observer.
-- Dividers are narrow windows (movable with acapi.setWindow, no library part needed).
-- On wall Change/Edit the proportional positions are recomputed; pure wall moves
-- are automatic no-ops (owned windows travel with the wall + setWindow is change-detected).

local watchedWall = nil
local dividers = {} -- divider window guids, in wall order
local DIV_COUNT = 10

local function wallLength(wall)
    local dx = wall.endC.x - wall.begC.x
    local dy = wall.endC.y - wall.begC.y
    return math.sqrt(dx * dx + dy * dy)
end

local function dividerParams(wall, L, i, n)
    local h = math.min(1.0, wall.height * 0.5)
    local sill = (wall.height - h) / 2
    if sill < 0 then sill = 0 end
    return {
        objLoc = L * (i - 0.5) / n,
        height = h,
        width = 0.15,
        sillHeight = sill,
        refSide = "inside",
        oSide = "inside",
        mirrored = false,
        openingAngle = 90
    }
end

local function placeDividers(wallGuid)
    local wall = acapi.getWall(wallGuid)
    if not wall then
        return false, "getWall failed"
    end
    local L = wallLength(wall)
    if L <= 0 then
        return false, "wall has zero length"
    end
    acapi.beginUndo("Place dividers")
    dividers = {}
    for i = 1, DIV_COUNT do
        local g, err = acapi.addWindow(wallGuid, dividerParams(wall, L, i, DIV_COUNT))
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
    local L = wallLength(wall)
    acapi.beginUndo("Sync dividers")
    for i, g in ipairs(dividers) do
        acapi.setWindow(g, { objLoc = L * (i - 0.5) / DIV_COUNT })
    end
    acapi.endUndo()
    return true
end

function onDividersWallEvent(guid, kwargs, kind)
    if kind ~= "edit" and kind ~= "change" then
        return
    end
    if refreshDividers(guid) then
        SetWebResult(string.format("Dividers synced (%s): %d ticks", tostring(kind), #dividers))
    else
        SetWebResult("Dividers sync failed (" .. tostring(kind) .. ")")
    end
end

RegisterWebEvent("onPlaceDividers", function()
    local guid = PickWall()
    if not guid then
        SetWebResult("Pick cancelled")
        return
    end
    watchedWall = guid
    local ok, err = placeDividers(guid)
    if not ok then
        SetWebResult("place failed: " .. tostring(err))
        return
    end
    local wok, werr = acapi.watch(guid, "try_dividers.lua\\onDividersWallEvent", { count = DIV_COUNT })
    if wok then
        ExecuteJS("document.getElementById('result').textContent='Watching wall with " .. DIV_COUNT .. " dividers — move it.';")
        SetWebResult(string.format("Placed %d dividers, watching wall", #dividers))
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
        SetWebResult(string.format("Refreshed %d dividers manually", #dividers))
    else
        SetWebResult("Refresh failed")
    end
end)

RegisterWebEvent("onUnwatchDividers", function()
    if watchedWall then
        acapi.unwatch(watchedWall)
        SetWebResult("Unwatched (dividers stay in the wall)")
        watchedWall = nil
    else
        SetWebResult("Nothing watched")
    end
end)

ShowWebDialog([[
<html><head><style>
body{background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:16px;margin:0;}
button{background:#0e639c;color:#fff;border:none;padding:8px 16px;font-size:14px;cursor:pointer;border-radius:3px;margin:0 8px 8px 0;}
</style></head><body>
<button onclick="archilua.DispatchEvent('onPlaceDividers')">Pick Wall + Place Dividers</button>
<button onclick="archilua.DispatchEvent('onRefreshDividers')">Refresh Now</button>
<button onclick="archilua.DispatchEvent('onUnwatchDividers')">Unwatch</button>
<div id='result'>Pick a wall to place 10 divider ticks.</div>
<script></script></body></html>
]])
