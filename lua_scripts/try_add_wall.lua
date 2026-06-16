print("--- Phase 3.5: Adding a new wall ---")

local guid = acapi.addwall({
    begC = { x = 0, y = 0 },
    endC = { x = 5, y = 0 },
    height = 3.0,
    thickness = 0.25,
    layer = 1,
    floor = 1
})

if guid then
    print("Created wall: " .. guid)

    local wall = acapi.getwall(guid)
    if wall then
        print("  height: " .. wall.height)
        print("  thickness: " .. wall.thickness)
        print("  begC: (" .. wall.begC.x .. ", " .. wall.begC.y .. ")")
        print("  endC: (" .. wall.endC.x .. ", " .. wall.endC.y .. ")")
    end
else
    print("Failed to create wall")
end

print("--- End ---")
