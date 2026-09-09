print("--- Phase 3.5: Adding a new wall with a window ---")

local wallGuid = acapi.addWall({
    begC = { x = 0, y = 0 },
    endC = { x = 5, y = 0 },
    height = 3.0,
    thickness = 0.25,
    layer = 1,
    floor = 0
})

if wallGuid then
    print("Created wall: " .. wallGuid)

    local wall = acapi.getWall(wallGuid)
    if wall then
        print("  height: " .. wall.height)
        print("  thickness: " .. wall.thickness)
        print("  begC: (" .. wall.begC.x .. ", " .. wall.begC.y .. ")")
        print("  endC: (" .. wall.endC.x .. ", " .. wall.endC.y .. ")")

        -- Place a window 2m from the start of the wall
        local winGuid = acapi.addWindow(wallGuid, {
            objLoc = 2.0,
            height = 1.5,
            width = 1.0,
            sillHeight = 0.9,
            refSide = "outside",
            oSide = "outside",
            mirrored = false
        })
        if winGuid then
            print("  window: " .. winGuid .. " (at 2m)")
        end

        -- Place a door 4m from the start of the wall, inside, mirrored
        local doorGuid = acapi.addDoor(wallGuid, {
            objLoc = 4.0,
            height = 2.0,
            width = 0.9,
            refSide = "inside",
            oSide = "inside",
            mirrored = true
        })
        if doorGuid then
            print("  door: " .. doorGuid .. " (at 4m, inside, mirrored)")
        end
    end
else
    print("Failed to create wall")
end

print("--- End ---")
