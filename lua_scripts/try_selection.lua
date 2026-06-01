local sel = acapi.getsel()
print("Selected " .. #sel .. " element(s)")
print("")

for i, guid in ipairs(sel) do
    local e = acapi.get(guid)
    print("[" .. i .. "] " .. e.typeName .. "  guid=" .. e.guid .. "  layer=" .. e.layer)

    local poly = acapi.getpoly(guid)
    if poly then
        print("      polygon: " .. #poly .. " vertices")
        for j, pt in ipairs(poly) do
            if j <= 3 or j > #poly - 2 then
                print("        [" .. j .. "] (" .. pt.x .. ", " .. pt.y .. ")")
            elseif j == 4 then
                print("        ...")
            end
        end
    else
        print("      (no polygon)")
    end
    print("")
end

print("--- done ---")