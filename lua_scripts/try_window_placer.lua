-- try_window_placer.lua — pick a wall, place a window via GUI
-- Everything in one file: HTML, JS, and Lua logic.

local selectedWall = nil

local function HandlePickWall()
    local guid = PickWall()
    if guid then
        selectedWall = guid
        ExecuteJS("document.getElementById('wallInfo').textContent='Wall: " .. guid .. "';")
        SetWebResult("Selected wall: " .. guid)
    else
        SetWebResult("Pick cancelled")
    end
end

local function HandlePlaceWindow()
    if not selectedWall then
        SetWebResult("Pick a wall first!")
        return
    end
    local qs = GetEventPayload()
    if qs == "" then
        SetWebResult("No payload received")
        return
    end

    local p = {}
    for k, v in qs:gmatch("([^=]+)=([^&]*)") do
        local n = tonumber(v)
        p[k] = (n ~= nil) and n or (v == "true") and true or (v == "false") and false or v
    end

    local ok, err = acapi.addWindow(selectedWall, {
        objLoc    = p.objLoc or 2.0,
        height    = p.height or 1.5,
        width     = p.width or 1.0,
        sillHeight= p.sillHeight or 0.9,
        wallSide  = p.wallSide or "inside",
        mirrored  = p.mirrored == true
    })
    if ok then
        SetWebResult("Window placed!")
    else
        SetWebResult("Error: " .. tostring(err))
    end
end

RegisterWebEvent("onPickWall", HandlePickWall)
RegisterWebEvent("onPlaceWindow", HandlePlaceWindow)

ShowWebDialog([[
<html>
<head>
<style>
body{background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:16px;margin:0;}
h2{margin:0 0 12px;font-size:16px;}
button{background:#0e639c;color:#fff;border:none;padding:8px 16px;font-size:14px;cursor:pointer;border-radius:3px;margin-right:8px;}
button:hover{background:#1177bb;}
#wallInfo{margin:8px 0;padding:6px 8px;background:#2d2d2d;border-radius:3px;font-size:12px;word-break:break-all;}
#result{margin-top:12px;padding:8px;background:#2d2d2d;border-radius:3px;font-size:13px;}
label{display:inline-block;width:90px;font-size:12px;}
input{margin:4px 0;width:80px;text-align:right;}
select{margin:4px 0;}
.row{margin:4px 0;}
</style>
</head>
<body>
<h2>Window Placer</h2>
<div><button onclick='pickWall()'>Pick Wall</button></div>
<div id='wallInfo'>No wall selected.</div>
<hr>
<div class='row'><label>Position (m):</label><input id='objLoc' type='number' value='2.0' step='0.1'></div>
<div class='row'><label>Height (m):</label><input id='height' type='number' value='1.5' step='0.1'></div>
<div class='row'><label>Width (m):</label><input id='width' type='number' value='1.0' step='0.1'></div>
<div class='row'><label>Sill (m):</label><input id='sillHeight' type='number' value='0.9' step='0.1'></div>
<div class='row'><label>Side:</label>
  <select id='wallSide'>
    <option value='inside'>Inside</option>
    <option value='outside'>Outside</option>
  </select>
</div>
<div class='row'><label>Mirrored:</label><input id='mirrored' type='checkbox'></div>
<div><button onclick='placeWindow()'>Place Window</button></div>
<div id='result'>Select a wall, set parameters, then place.</div>
<script>
function pickWall(){
    document.getElementById('result').textContent='Click a wall in the ArchiCAD viewport...';
    archilua.DispatchEvent('onPickWall');
}
function placeWindow(){
    var f = function(id){ return document.getElementById(id); };
    var qs = 'objLoc=' + f('objLoc').value
           + '&height=' + f('height').value
           + '&width=' + f('width').value
           + '&sillHeight=' + f('sillHeight').value
           + '&wallSide=' + f('wallSide').value
           + '&mirrored=' + f('mirrored').checked;
    archilua.SendEvent('onPlaceWindow', qs);
}
</script>
</body>
</html>
]])
