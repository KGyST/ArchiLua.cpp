-- try_web_gui.lua — self-contained plugin: registers event, defines HTML,
-- shows the palette. All UI logic lives in this single file.

RegisterWebEvent("onPickWall", function()
    local guid = PickWall()
    if guid then
        SetWebResult("GUID: " .. guid)
    else
        SetWebResult("Cancelled")
    end
end)

-- Pull the HTML to the dialog.
-- The [[ ]] syntax keeps it readable within Lua.
ShowWebDialog([[
<html>
<head>
<style>
body{background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:16px;margin:0;}
button{background:#0e639c;color:#fff;border:none;padding:8px 16px;font-size:14px;cursor:pointer;border-radius:3px;}
button:hover{background:#1177bb;}
#result{margin-top:12px;padding:8px;background:#2d2d2d;border-radius:3px;font-size:13px;white-space:pre-wrap;word-break:break-all;}
</style>
</head>
<body>
<button id='btnPick' onclick='pickWall()'>Pick Wall</button>
<div id='result'>Press button then click a wall in ArchiCAD.</div>
<script>
function pickWall(){
  document.getElementById('result').textContent='Click a wall in the ArchiCAD viewport...';
  archilua.DispatchEvent('onPickWall');
}
</script>
</body>
</html>
]])
