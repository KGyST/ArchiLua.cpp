-- try_window_placer.lua — pick a wall, place or modify windows via GUI

local selectedWall = nil
local selectedWindow = nil

local function HandlePickWall()
    local guid = PickWall()
    if guid then
        selectedWall = guid
        selectedWindow = nil
        local wallData = acapi.getWall(guid)
        local windowsJson = "[]"
        local nWindows = 0
        if wallData and wallData.openings and wallData.openings.windows then
            local items = {}
            for i, wGuid in ipairs(wallData.openings.windows) do
                table.insert(items, "\"" .. wGuid .. "\"")
            end
            nWindows = #wallData.openings.windows
            windowsJson = "[" .. table.concat(items, ",") .. "]"
        end
        ExecuteJS("onWallPicked('" .. guid .."', " .. windowsJson .. ");")
        SetWebResult("Selected wall: " .. guid .. " (" .. nWindows .. " windows)")
    else
        SetWebResult("Pick cancelled")
    end
end

local function HandleGetWindow(args)
    if not args or not args.guid or args.guid == "" then
        SetWebResult("No window GUID provided")
        return
    end
    selectedWindow = args.guid
    local winData = acapi.getWindow(args.guid)
    if winData then
        local json = string.format(
            '{"objLoc":%f,"height":%f,"width":%f,"sillHeight":%f,"refSide":"%s","oSide":"%s","mirrored":%s,"openingAngle":%f}',
            winData.objLoc or 0,
            winData.height or 1.5,
            winData.width or 1.0,
            winData.sillHeight or 0.9,
            winData.refSide or "inside",
            winData.oSide or "inside",
            tostring(winData.mirrored or false),
            winData.openingAngle or 45
        )
        ExecuteJS("populateWindow(" .. json .. ");")
        SetWebResult("Loaded window: " .. args.guid)
    else
        SetWebResult("Failed to get window data for: " .. args.guid)
    end
end

local function HandleSaveWindow(args)
    if not selectedWall then
        SetWebResult("Pick a wall first!")
        return
    end
    if not args then
        SetWebResult("No args received")
        return
    end

    args.objLoc = args.objLoc or 2.0
    args.height = args.height or 1.5
    args.width = args.width or 1.0
    args.sillHeight = args.sillHeight or 0.9
    args.refSide = args.refSide or "inside"
    args.oSide = args.oSide or "inside"
    args.mirrored = args.mirrored or false
    args.openingAngle = args.openingAngle or 45

    -- Use the dropdown selection sent from JS, not the stale global,
    -- so "Place New Window" really creates instead of updating the last window.
    local target = (args.guid and args.guid ~= "") and args.guid or nil
    if target then
        selectedWindow = target
        local ok, err = acapi.setWindow(target, args)
        if ok then
            SetWebResult("Window updated successfully!")
        else
            SetWebResult("Error updating window: " .. tostring(err))
        end
    else
        local guid, err = acapi.addWindow(selectedWall, args)
        if guid then
            selectedWindow = guid
            SetWebResult("Window placed! GUID: " .. guid)
            -- Refresh window list
            local wallData = acapi.getWall(selectedWall)
            if wallData and wallData.openings and wallData.openings.windows then
                local items = {}
                for i, wGuid in ipairs(wallData.openings.windows) do
                    table.insert(items, "\"" .. wGuid .. "\"")
                end
                ExecuteJS("updateWindowList(" .. "[" .. table.concat(items, ",") .. "]" .. ", '" .. guid .. "');")
                ExecuteJS("document.getElementById('saveBtn').textContent='Update Window';")
            end
        else
            SetWebResult("Error adding window: " .. tostring(err))
        end
    end
end

RegisterWebEvent("onPickWall", HandlePickWall)
RegisterWebEvent("onGetWindow", HandleGetWindow)
RegisterWebEvent("onSaveWindow", HandleSaveWindow)

ShowWebDialog([[
<html>
<head>
<style>
body{background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:16px;margin:0;}
h2{margin:0 0 12px;font-size:16px;}
button{background:#0e639c;color:#fff;border:none;padding:8px 16px;font-size:14px;cursor:pointer;border-radius:3px;margin-right:8px;}
button:hover{background:#1177bb;}
#wallInfo,#windowInfo{margin:8px 0;padding:6px 8px;background:#2d2d2d;border-radius:3px;font-size:12px;word-break:break-all;}
#result{margin-top:12px;padding:8px;background:#2d2d2d;border-radius:3px;font-size:13px;}
label{display:inline-block;width:90px;font-size:12px;}
input{margin:4px 0;width:80px;text-align:right;}
select{margin:4px 0;max-width:180px;}
.row{margin:4px 0;}
</style>
</head>
<body>
<h2>Window Placer & Modifier</h2>
<div><button onclick='pickWall()'>Pick Wall</button></div>
<div id='wallInfo'>No wall selected.</div>
<div class='row' style='margin-top:8px;'>
  <label>Windows:</label>
  <select id='windowSelect' onchange='selectWindow(this.value)'>
    <option value=''>-- New Window --</option>
  </select>
</div>
<hr>
<div class='row'><label>Position (m):</label><input id='objLoc' type='number' value='2.0' step='0.1'></div>
<div class='row'><label>Height (m):</label><input id='height' type='number' value='1.5' step='0.1'></div>
<div class='row'><label>Width (m):</label><input id='width' type='number' value='1.0' step='0.1'></div>
<div class='row'><label>Sill (m):</label><input id='sillHeight' type='number' value='0.9' step='0.1'></div>
<div class='row'><label>RefSide:</label>
  <select id='refSide'>
    <option value='inside'>Inside</option>
    <option value='outside'>Outside</option>
  </select>
</div>
<div class='row'><label>oSide:</label>
  <select id='oSide'>
    <option value='inside'>Inside</option>
    <option value='outside'>Outside</option>
  </select>
</div>
<div class='row'><label>Mirrored:</label><input id='mirrored' type='checkbox'></div>
<div class='row'><label>Opening &deg;:</label><input id='openingAngle' type='number' value='45' min='0' max='180' step='5'></div>
<div style='margin-top:8px;'><button id='saveBtn' onclick='saveWindow()'>Place New Window</button></div>
<div id='result'>Select a wall, set parameters, then place or modify.</div>
<script>
function pickWall(){
    document.getElementById('result').textContent='Click a wall in the ArchiCAD viewport...';
    archilua.DispatchEvent('onPickWall');
}
function onWallPicked(wallGuid, windows){
    document.getElementById('wallInfo').textContent='Wall: ' + wallGuid;
    updateWindowList(windows, '');
}
function updateWindowList(windows, selectedGuid){
    var sel = document.getElementById('windowSelect');
    sel.innerHTML = "<option value=''>-- New Window --</option>";
    for(var i=0; i<windows.length; i++){
        var opt = document.createElement('option');
        opt.value = windows[i];
        opt.textContent = 'Window ' + (i+1) + ' (' + windows[i].substring(0,8) + '...)';
        if(windows[i] === selectedGuid) opt.selected = true;
        sel.appendChild(opt);
    }
}
function selectWindow(guid){
    if(!guid){
        document.getElementById('saveBtn').textContent = 'Place New Window';
        return;
    }
    document.getElementById('saveBtn').textContent = 'Update Window';
    archilua.CallLua('onGetWindow', JSON.stringify({guid: guid}));
}
function populateWindow(data){
    var f = function(id){ return document.getElementById(id); };
    f('objLoc').value = data.objLoc;
    f('height').value = data.height;
    f('width').value = data.width;
    f('sillHeight').value = data.sillHeight;
    f('refSide').value = data.refSide;
    f('oSide').value = data.oSide;
    f('mirrored').checked = data.mirrored;
    f('openingAngle').value = data.openingAngle;
}
function saveWindow(){
    var f = function(id){ return document.getElementById(id); };
    var winGuid = f('windowSelect').value;
    archilua.CallLua('onSaveWindow', JSON.stringify({
        guid: winGuid,
        objLoc: parseFloat(f('objLoc').value),
        height: parseFloat(f('height').value),
        width: parseFloat(f('width').value),
        sillHeight: parseFloat(f('sillHeight').value),
        refSide: f('refSide').value,
        oSide: f('oSide').value,
        mirrored: f('mirrored').checked,
        openingAngle: parseFloat(f('openingAngle').value)
    }));
}
</script>
</body>
</html>
]])
