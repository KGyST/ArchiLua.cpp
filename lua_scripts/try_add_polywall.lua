print("--- Phase 3.5: Adding a polygonal wall ---")

local wallGuid = acapi.addWall({
    poly = {
        { x = 0, y = 0 },
        { x = 3, y = 0 },
        { x = 4, y = 2 },
        { x = 1, y = 2 }
    },
    height = 3.0,
    thickness = 0.25,
    layer = 1,
    floor = 0
})

if wallGuid then
    print("Created polygonal wall: " .. wallGuid)

    local wall = acapi.getWall(wallGuid)
    if wall then
        print("  type: " .. wall.type)
        print("  height: " .. wall.height)
        print("  thickness: " .. wall.thickness)
        print("  coords: " .. #wall.coords .. " points")
        for i, pt in ipairs(wall.coords) do
            print("    [" .. i .. "] (" .. pt.x .. ", " .. pt.y .. ")")
        end
    end
else
    print("Failed to create polygonal wall")
end

print("--- End ---")
