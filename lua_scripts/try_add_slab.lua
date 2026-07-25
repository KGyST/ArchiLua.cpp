print("--- Phase 3.5: Adding a slab ---")

local slabGuid = acapi.addSlab({
    poly = {
        { x = 0, y = 0 },
        { x = 5, y = 0 },
        { x = 5, y = 4 },
        { x = 0, y = 4 }
    },
    thickness = 0.3,
    layer = 1,
    floor = 0
})

if slabGuid then
    print("Created slab: " .. slabGuid)

    local elem = acapi.get(slabGuid)
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
    print("Failed to create slab")
end

print("--- End ---")
