$root = $PSScriptRoot
$incs = @(
    "/ISrc",
    "/ICommonLibs.cpp",
    "/I../support/archicad-buildsupport/AC27/API/Support/Inc",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/RS",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/CADInfrastructureBase",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/Graphix",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/GSRoot",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/GSUtils",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/DGLib",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/JavascriptEngine",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/Geometry",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/InputOutput",
    "/I../support/archicad-buildsupport/AC27/API/Support/Modules/UCLib",
    "/IE:/Git/ArchiLua.cpp/deps/build/_deps/lua-src/src",
    "/D_ITERATOR_DEBUG_LEVEL=2",
    "/D_DEBUG",
    "/DACVER=27",
    "/D_STLP_DONT_FORCE_MSVC_LIB_NAME",
    "/D_WINDLL",
    "/D_UNICODE",
    "/DUNICODE",
    "/std:c++17",
    "/Zc:wchar_t-",
    "/EHsc",
    "/MDd",
    "/W4",
    "/c"
)
$files = @(
    "Src/ArchiLua.cpp",
    "Src/Gui/LuaScriptDialog.cpp",
    "Src/Gui/LuaWebDialog.cpp"
)

$entries = foreach ($f in $files) {
    @{
        directory = $root
        file = "$root/$f"
        arguments = @("clang-cl.exe") + $incs + @($f)
    }
}
ConvertTo-Json $entries -Depth 3 | Out-File -FilePath "$root/compile_commands.json" -Encoding ascii
Write-Host "Generated compile_commands.json"