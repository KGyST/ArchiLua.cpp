-- try_observer.lua — Phase 3.7 PoC test (standalone, independent from window placer)
local eventCount = 0

function onWallEvent(guid, kwargs, kind)
    eventCount = eventCount + 1
    local note = (kwargs and kwargs.note) or "?"
    SetWebResult(string.format("#%d kind=%s guid=%s note=%s", eventCount, tostring(kind), tostring(guid), tostring(note)))
end

RegisterWebEvent("onPickWall", function()
    local guid = PickWall()
    if guid then
        local ok, err = acapi.watch(guid, "try_observer.lua\\onWallEvent", { note = "drag-me" })
        acapi.observerLog(true) -- raw notifID sequence goes to the Report window
        if ok then
            ExecuteJS("document.getElementById('result').textContent='Watching: " .. guid .. " — drag the wall now.';")
            SetWebResult("Watching wall: " .. guid)
        else
            SetWebResult("watch failed: " .. tostring(err))
        end
    else
        SetWebResult("Pick cancelled")
    end
end)

RegisterWebEvent("onUnwatch", function()
    local guid = PickWall()
    if guid then
        acapi.unwatch(guid)
        SetWebResult("Unwatched: " .. guid)
    end
end)

ShowWebDialog([[
<html><head><style>
body{background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:16px;margin:0;}
button{background:#0e639c;color:#fff;border:none;padding:8px 16px;font-size:14px;cursor:pointer;border-radius:3px;margin-right:8px;}
</style></head><body>
<button onclick="archilua.DispatchEvent('onPickWall')">Pick + Watch Wall</button>
<button onclick="archilua.DispatchEvent('onUnwatch')">Pick + Unwatch</button>
<div id='result'>Pick a wall, then drag it in the viewport.</div>
<script></script></body></html>
]])
