@echo off
REM Generate compile_commands.json for clang-tidy static analysis
REM This file should be kept in sync with the vcxproj ClCompile includes.

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "INC_SRC=/ISrc"
set "INC_COMMON=/ICommonLibs.cpp"
set "INC_AC27=/I..\support\archicad-buildsupport\AC27\API\Support\Inc"
set "INC_RS=/I..\support\archicad-buildsupport\AC27\API\Support\Modules\RS"
set "INC_CAD=/I..\support\archicad-buildsupport\AC27\API\Support\Modules\CADInfrastructureBase"
set "INC_GFX=/I..\support\archicad-buildsupport\AC27\API\Support\Modules\Graphix"
set "INC_GSROOT=/I..\support\archicad-buildsupport\AC27\API\Support\Modules\GSRoot"
set "INC_GSUTILS=/I..\support\archicad-buildsupport\AC27\API\Support\Modules\GSUtils"
set "INC_DG=/I..\support\archicad-buildsupport\AC27\API\Support\Modules\DGLib"
set "INC_GEOM=/I..\support\archicad-buildsupport\AC27\API\Support\Modules\Geometry"
set "INC_IO=/I..\support\archicad-buildsupport\AC27\API\Support\Modules\InputOutput"
set "INC_UCLIB=/I..\support\archicad-buildsupport\AC27\API\Support\Modules\UCLib"
set "INC_LUA=/IE:\Git\ArchiLua.cpp\deps\build\_deps\lua-src\src"
set "DEFINES=/D_ITERATOR_DEBUG_LEVEL=2 /D_DEBUG /DACVER=27 /D_STLP_DONT_FORCE_MSVC_LIB_NAME /D_WINDLL /D_UNICODE /DUNICODE"
set "FLAGS=/std:c++17 /Zc:wchar_t- /EHsc /MDd /W4 /c"

set "COMMON_ARGS=%INC_SRC% %INC_COMMON% %INC_AC27% %INC_RS% %INC_CAD% %INC_GFX% %INC_GSROOT% %INC_GSUTILS% %INC_DG% %INC_GEOM% %INC_IO% %INC_UCLIB% %INC_LUA% %DEFINES% %FLAGS%"

echo [{" > "%ROOT%\compile_commands.json"
echo   "directory": "%ROOT:\=\\%", >> "%ROOT%\compile_commands.json"
echo   "file": "%ROOT:\=\\%\\Src\\ArchiLua.cpp", >> "%ROOT%\compile_commands.json"
echo   "arguments": ["clang-cl.exe", >> "%ROOT%\compile_commands.json"
for %%a in (%COMMON_ARGS%) do echo   "%%a", >> "%ROOT%\compile_commands.json"
echo   "Src/ArchiLua.cpp"] >> "%ROOT%\compile_commands.json"
echo }, >> "%ROOT%\compile_commands.json"
echo { >> "%ROOT%\compile_commands.json"
echo   "directory": "%ROOT:\=\\%", >> "%ROOT%\compile_commands.json"
echo   "file": "%ROOT:\=\\%\\Src\\Gui\\LuaScriptDialog.cpp", >> "%ROOT%\compile_commands.json"
echo   "arguments": ["clang-cl.exe", >> "%ROOT%\compile_commands.json"
for %%a in (%COMMON_ARGS%) do echo   "%%a", >> "%ROOT%\compile_commands.json"
echo   "Src/Gui/LuaScriptDialog.cpp"] >> "%ROOT%\compile_commands.json"
echo }] >> "%ROOT%\compile_commands.json"

echo Generated compile_commands.json
