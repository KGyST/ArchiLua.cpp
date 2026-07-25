print("--- Phase 3.5: Adding a roof ---")

local roofGuid = acapi.addRoof({
    poly = {
        { x = 0, y = 0 },
        { x = 6, y = 0 },
        { x = 6, y = 5 },
        { x = 0, y = 5 }
    },
    thickness = 0.25,
    layer = 1,
    floor = 1
})

if roofGuid then
    print("Created roof: " .. roofGuid)

    local elem = acapi.get(roofGuid)
    if elem then
        print("  type: " .. elem.typeName)
        print("  layer: " .. elem.layer)
        if elem.coords then
            print("  coords: " .. #elem.coords .. " points")
            for i, pt in ipairs(elem.coords) do
                print("    [" .. i .. "] (" .. pt.x .. ", " .. pt.y .. ")")
            end
        end
    end
else
    print("Failed to create roof")
end

print("--- End ---")
