#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "ACAPinc.h"

namespace ArchiLua {
namespace APIModule {

static void PushCoord(lua_State* L, double x, double y)
{
    lua_createtable(L, 0, 2);
    lua_pushnumber(L, x);  lua_setfield(L, -2, "x");
    lua_pushnumber(L, y);  lua_setfield(L, -2, "y");
}

static void PushGuid(lua_State* L, const API_Guid& guid)
{
    GS::UniString s = APIGuidToString(guid);
    lua_pushstring(L, s.ToCStr().Get());
}

static const char* ElementTypeName(API_ElemTypeID typeID)
{
    switch (typeID) {
        case API_WallID:     return "Wall";
        case API_ColumnID:   return "Column";
        case API_BeamID:     return "Beam";
        case API_WindowID:   return "Window";
        case API_DoorID:     return "Door";
        case API_ObjectID:   return "Object";
        case API_SlabID:     return "Slab";
        case API_RoofID:     return "Roof";
        case API_MeshID:     return "Mesh";
        case API_ZoneID:     return "Zone";
        case API_ShellID:    return "Shell";
        default:             return "Other";
    }
}

static int GetSelection(lua_State* L)
{
    API_SelectionInfo selInfo;
    GS::Array<API_Neig> selNeigs;

    GSErrCode err = ACAPI_Selection_Get(&selInfo, &selNeigs, true);
    if (err == APIERR_NOSEL) {
        lua_newtable(L);
        return 1;
    }
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushstring(L, "ACAPI_Selection_Get failed");
        return 2;
    }

    lua_createtable(L, (int)selNeigs.GetSize(), 0);
    for (UIndex i = 0; i < selNeigs.GetSize(); ++i) {
        PushGuid(L, selNeigs[i].guid);
        lua_rawseti(L, -2, (int)(i + 1));
    }
    return 1;
}

static int GetWall(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (!guidStr) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a GUID string argument");
        return 2;
    }

    API_Guid guid = APIGuidFromString(guidStr);

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get(&elem);
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushstring(L, "ACAPI_Element_Get failed");
        return 2;
    }

    if (elem.header.type.typeID != API_WallID) {
        lua_pushnil(L);
        lua_pushstring(L, "element is not a wall");
        return 2;
    }

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));
    err = ACAPI_Element_GetMemo(guid, &memo,
        APIMemoMask_Polygon |
        APIMemoMask_WallWindows |
        APIMemoMask_WallDoors);
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushstring(L, "ACAPI_Element_GetMemo failed");
        return 2;
    }

    lua_createtable(L, 0, 8);

    PushGuid(L, elem.header.guid);
    lua_setfield(L, -2, "guid");

    lua_pushinteger(L, elem.header.layer.ToInt32_Deprecated());
    lua_setfield(L, -2, "layer");

    const char* wallTypeName = "Normal";
    switch (elem.wall.type) {
        case APIWtyp_Trapez: wallTypeName = "Trapez"; break;
        case APIWtyp_Poly:   wallTypeName = "Poly";   break;
    }
    lua_pushstring(L, wallTypeName);
    lua_setfield(L, -2, "type");

    lua_pushnumber(L, elem.wall.height);
    lua_setfield(L, -2, "height");

    lua_pushnumber(L, elem.wall.thickness);
    lua_setfield(L, -2, "thickness");

    PushCoord(L, elem.wall.begC.x, elem.wall.begC.y);
    lua_setfield(L, -2, "begC");

    PushCoord(L, elem.wall.endC.x, elem.wall.endC.y);
    lua_setfield(L, -2, "endC");

    if (memo.coords != nullptr) {
        Int32 nCoords = elem.wall.poly.nCoords;
        lua_createtable(L, nCoords - 1, 0);
        for (Int32 i = 1; i < nCoords; ++i) {
            PushCoord(L, (*memo.coords)[i].x, (*memo.coords)[i].y);
            lua_rawseti(L, -2, i);
        }
        lua_setfield(L, -2, "coords");
    }

    lua_createtable(L, 0, 2);

    if (memo.wallWindows != nullptr) {
        GSSize nWindows = BMGetPtrSize(reinterpret_cast<GSPtr>(memo.wallWindows)) / sizeof(API_Guid);
        lua_createtable(L, (int)nWindows, 0);
        for (int i = 0; i < (int)nWindows; ++i) {
            PushGuid(L, memo.wallWindows[i]);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, "windows");
    }

    if (memo.wallDoors != nullptr) {
        GSSize nDoors = BMGetPtrSize(reinterpret_cast<GSPtr>(memo.wallDoors)) / sizeof(API_Guid);
        lua_createtable(L, (int)nDoors, 0);
        for (int i = 0; i < (int)nDoors; ++i) {
            PushGuid(L, memo.wallDoors[i]);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, "doors");
    }

    lua_setfield(L, -2, "openings");

    ACAPI_DisposeElemMemoHdls(&memo);
    return 1;
}

static int GetElement(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (!guidStr) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a GUID string argument");
        return 2;
    }

    API_Guid guid = APIGuidFromString(guidStr);

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get(&elem);
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushstring(L, "ACAPI_Element_Get failed");
        return 2;
    }

    lua_createtable(L, 0, 8);
    PushGuid(L, elem.header.guid);
    lua_setfield(L, -2, "guid");
    lua_pushinteger(L, elem.header.layer.ToInt32_Deprecated());
    lua_setfield(L, -2, "layer");
    lua_pushstring(L, ElementTypeName(elem.header.type.typeID));
    lua_setfield(L, -2, "typeName");

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));
    GSErrCode memoErr = ACAPI_Element_GetMemo(guid, &memo, APIMemoMask_Polygon);
    if (memoErr == NoError && memo.coords != nullptr) {
        GSSize total = BMGetPtrSize(reinterpret_cast<GSPtr>(memo.coords)) / sizeof(API_Coord);
        Int32 nCoords = (Int32)total;
        if (nCoords > 1) {
            lua_createtable(L, nCoords - 1, 0);
            for (Int32 i = 1; i < nCoords; ++i) {
                PushCoord(L, (*memo.coords)[i].x, (*memo.coords)[i].y);
                lua_rawseti(L, -2, i);
            }
            lua_setfield(L, -2, "coords");
        }
        ACAPI_DisposeElemMemoHdls(&memo);
    }

    return 1;
}

static int GetPoly(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (!guidStr) {
        lua_pushnil(L);
        return 1;
    }

    API_Guid guid = APIGuidFromString(guidStr);

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.guid = guid;
    if (ACAPI_Element_Get(&elem) != NoError) {
        lua_pushnil(L);
        return 1;
    }

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));
    if (ACAPI_Element_GetMemo(guid, &memo, APIMemoMask_Polygon) != NoError || memo.coords == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    GSSize total = BMGetPtrSize(reinterpret_cast<GSPtr>(memo.coords)) / sizeof(API_Coord);
    Int32 nCoords = (Int32)total;

    if (nCoords <= 1) {
        ACAPI_DisposeElemMemoHdls(&memo);
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, nCoords - 1, 0);
    for (Int32 i = 1; i < nCoords; ++i) {
        PushCoord(L, (*memo.coords)[i].x, (*memo.coords)[i].y);
        lua_rawseti(L, -2, i);
    }

    ACAPI_DisposeElemMemoHdls(&memo);
    return 1;
}

inline void Register(lua_State* L)
{
    lua_newtable(L);

    lua_pushcfunction(L, GetSelection);
    lua_setfield(L, -2, "getsel");

    lua_pushcfunction(L, GetWall);
    lua_setfield(L, -2, "getwall");

    lua_pushcfunction(L, GetElement);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, GetPoly);
    lua_setfield(L, -2, "getpoly");

    lua_setglobal(L, "acapi");
}

} // namespace APIModule
} // namespace ArchiLua