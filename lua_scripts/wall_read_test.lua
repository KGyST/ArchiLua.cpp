print("--- Phase 2: Wall Read Test ---")

local sel = acapi.getsel()
print("Selected elements: " .. #sel)

for i, guid in ipairs(sel) do
    print("  [" .. i .. "] " .. guid)

    local wall = acapi.getwall(guid)
    if wall then
        print("    type: " .. wall.type)
        print("    layer: " .. wall.layer)
        print("    height: " .. wall.height)
        print("    thickness: " .. wall.thickness)
        print("    begC: (" .. wall.begC.x .. ", " .. wall.begC.y .. ")")
        print("    endC: (" .. wall.endC.x .. ", " .. wall.endC.y .. ")")

        if wall.coords then
            print("    coords: " .. #wall.coords .. " points")
            for j, pt in ipairs(wall.coords) do
                if j <= 5 or j > #wall.coords - 2 then
                    print("      [" .. j .. "] (" .. pt.x .. ", " .. pt.y .. ")")
                elseif j == 6 then
                    print("      ...")
                end
            end
        end

        if wall.openings then
            if wall.openings.windows then
                print("    windows: " .. #wall.openings.windows)
                for j, wg in ipairs(wall.openings.windows) do
                    print("      [" .. j .. "] " .. wg)
                end
            end
            if wall.openings.doors then
                print("    doors: " .. #wall.openings.doors)
                for j, dg in ipairs(wall.openings.doors) do
                    print("      [" .. j .. "] " .. dg)
                end
            end
        end
    else
        print("    error: " .. guid .. " is not a wall")
    end
end

print("--- End ---")
