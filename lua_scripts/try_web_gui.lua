-- Web GUI demo: register event handlers, then show the web palette.
-- Menu 2 shortcut loads this directly.

RegisterWebEvent("onPickWall", function()
    local guid = PickWall()
    if guid then
        SetWebResult("GUID: " .. guid)
    else
        SetWebResult("Cancelled")
    end
end)

ShowWebDialog()
