#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "ACAPinc.h"
#include "../ArchiLua.hpp"

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
    BNZeroMemory(&selInfo, sizeof(selInfo));
    GS::Array<API_Neig> selNeigs;

    GSErrCode err = ACAPI_Selection_Get(&selInfo, &selNeigs, true);
    if (err == APIERR_NOSEL) {
        lua_newtable(L);
        return 1;
    }
    if (err != NoError) {
        lua_pushnil(L);
        char buf[64];
        std::sprintf(buf, "ACAPI_Selection_Get failed: err=%d", (int)err);
        lua_pushstring(L, buf);
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

static int GetParams(lua_State* L)
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

    Int32 libInd = 0;
    switch (elem.header.type.typeID) {
        case API_ObjectID:   libInd = elem.object.libInd;         break;
        case API_WindowID:   libInd = elem.window.openingBase.libInd;  break;
        case API_DoorID:     libInd = elem.door.openingBase.libInd;   break;
        case API_SkylightID: libInd = elem.skylight.openingBase.libInd; break;
        case API_LampID:     libInd = elem.lamp.libInd;           break;
        case API_ZoneID:     libInd = elem.zone.libInd;           break;
        default: break;
    }

    if (libInd == 0) {
        lua_pushnil(L);
        return 1;
    }

    API_ParamOwnerType paramOwner;
    BNZeroMemory(&paramOwner, sizeof(paramOwner));
    paramOwner.libInd = libInd;

    GSErrCode err = ACAPI_LibraryPart_OpenParameters(&paramOwner);
    if (err != NoError) {
        lua_pushnil(L);
        return 1;
    }

    API_GetParamsType getParams;
    BNZeroMemory(&getParams, sizeof(getParams));
    err = ACAPI_LibraryPart_GetActParameters(&getParams);
    if (err != NoError || getParams.params == nullptr) {
        ACAPI_LibraryPart_CloseParameters();
        lua_pushnil(L);
        return 1;
    }

    UInt32 nParams = (UInt32)(BMGetHandleSize((GSHandle)getParams.params) / sizeof(API_AddParType));
    lua_createtable(L, 0, (int)nParams);

    for (UInt32 i = 0; i < nParams; ++i) {
        const API_AddParType& par = (*getParams.params)[i];

        if (par.typeMod != API_ParSimple)
            continue;

        switch (par.typeID) {
            case APIParT_Integer:
            case APIParT_Length:
            case APIParT_Angle:
            case APIParT_RealNum:
            case APIParT_Intens:
                lua_pushnumber(L, par.value.real);
                break;
            case APIParT_LightSw:
            case APIParT_Boolean:
                lua_pushboolean(L, par.value.real != 0.0);
                break;
            case APIParT_CString:
            {
                int wlen = 0;
                while (wlen < API_UAddParStrLen && par.value.uStr[wlen] != 0)
                    ++wlen;
                if (wlen > 0) {
                    int mlen = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)par.value.uStr, wlen, nullptr, 0, nullptr, nullptr);
                    if (mlen > 0) {
                        std::string s(mlen, '\0');
                        WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)par.value.uStr, wlen, &s[0], mlen, nullptr, nullptr);
                        lua_pushstring(L, s.c_str());
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
                break;
            }
            default:
                continue;
        }
        lua_setfield(L, -2, par.name);
    }

    ACAPI_DisposeAddParHdl(&getParams.params);
    ACAPI_LibraryPart_CloseParameters();
    return 1;
}

// ---------------------------------------------------------------------------
// Writing functions (Phase 3)
// ---------------------------------------------------------------------------

static int BeginUndo(lua_State* L)
{
    const char* label = lua_tostring(L, 1);
    UndoBegin(label);
    return 0;
}

static int EndUndo(lua_State* L)
{
    GSErrCode err = UndoEnd();
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "endundo failed: err=%d", (int)err);
        return 2;
    }
    lua_pushboolean(L, true);
    return 1;
}

static int SetWall(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (!guidStr || !lua_istable(L, 2)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a GUID string and a table argument");
        return 2;
    }

    API_Guid guid = APIGuidFromString(guidStr);

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get(&elem);
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "ACAPI_Element_Get failed: err=%d", (int)err);
        return 2;
    }
    if (elem.header.type.typeID != API_WallID) {
        lua_pushnil(L);
        lua_pushstring(L, "element is not a wall");
        return 2;
    }

    API_Element mask;
    ACAPI_ELEMENT_MASK_CLEAR(mask);

    lua_pushnil(L);
    while (lua_next(L, 2)) {
        const char* key = lua_tostring(L, -2);
        if (!key) {
            lua_pop(L, 1);
            continue;
        }

        if (strcmp(key, "height") == 0 && lua_isnumber(L, -1)) {
            elem.wall.height = lua_tonumber(L, -1);
            ACAPI_ELEMENT_MASK_SET(mask, API_WallType, height);
        } else if (strcmp(key, "thickness") == 0 && lua_isnumber(L, -1)) {
            elem.wall.thickness = lua_tonumber(L, -1);
            ACAPI_ELEMENT_MASK_SET(mask, API_WallType, thickness);
        } else if (strcmp(key, "layer") == 0 && lua_isinteger(L, -1)) {
            elem.header.layer = ACAPI_CreateAttributeIndex((Int32)lua_tointeger(L, -1));
            ACAPI_ELEMENT_MASK_SET(mask, API_Elem_Head, layer);
        } else if (strcmp(key, "begC") == 0 && lua_istable(L, -1)) {
            lua_getfield(L, -1, "x");
            lua_getfield(L, -2, "y");
            if (lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
                elem.wall.begC.x = lua_tonumber(L, -2);
                elem.wall.begC.y = lua_tonumber(L, -1);
                ACAPI_ELEMENT_MASK_SET(mask, API_WallType, begC);
            }
            lua_pop(L, 2);
        } else if (strcmp(key, "endC") == 0 && lua_istable(L, -1)) {
            lua_getfield(L, -1, "x");
            lua_getfield(L, -2, "y");
            if (lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
                elem.wall.endC.x = lua_tonumber(L, -2);
                elem.wall.endC.y = lua_tonumber(L, -1);
                ACAPI_ELEMENT_MASK_SET(mask, API_WallType, endC);
            }
            lua_pop(L, 2);
        }

        lua_pop(L, 1);
    }

    if (UndoIsActive()) {
        UndoBuffer(guid, elem, mask);
    } else {
        err = ACAPI_CallUndoableCommand("Modify Wall", [&]() -> GSErrCode {
            return ACAPI_Element_Change(&elem, &mask, nullptr, 0, true);
        });
        if (err != NoError) {
            lua_pushnil(L);
            lua_pushfstring(L, "setwall failed: err=%d", (int)err);
            return 2;
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

static int SetElement(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (!guidStr || !lua_istable(L, 2)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a GUID string and a table argument");
        return 2;
    }

    API_Guid guid = APIGuidFromString(guidStr);

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get(&elem);
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "ACAPI_Element_Get failed: err=%d", (int)err);
        return 2;
    }

    API_Element mask;
    ACAPI_ELEMENT_MASK_CLEAR(mask);

    lua_pushnil(L);
    while (lua_next(L, 2)) {
        const char* key = lua_tostring(L, -2);
        if (!key) {
            lua_pop(L, 1);
            continue;
        }

        if (strcmp(key, "layer") == 0 && lua_isinteger(L, -1)) {
            elem.header.layer = ACAPI_CreateAttributeIndex((Int32)lua_tointeger(L, -1));
            ACAPI_ELEMENT_MASK_SET(mask, API_Elem_Head, layer);
        }

        lua_pop(L, 1);
    }

    if (UndoIsActive()) {
        UndoBuffer(guid, elem, mask);
    } else {
        err = ACAPI_CallUndoableCommand("Modify Element", [&]() -> GSErrCode {
            return ACAPI_Element_Change(&elem, &mask, nullptr, 0, true);
        });
        if (err != NoError) {
            lua_pushnil(L);
            lua_pushfstring(L, "set failed: err=%d", (int)err);
            return 2;
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

static int SetParams(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (!guidStr || !lua_istable(L, 2)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a GUID string and a table argument");
        return 2;
    }

    API_Guid guid = APIGuidFromString(guidStr);

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get(&elem);
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "ACAPI_Element_Get failed: err=%d", (int)err);
        return 2;
    }

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));
    err = ACAPI_Element_GetMemo(guid, &memo, APIMemoMask_AddPars);
    if (err != NoError || memo.params == nullptr) {
        lua_pushnil(L);
        lua_pushstring(L, "element has no modifiable parameters");
        return 2;
    }

    UInt32 nParams = (UInt32)(BMGetHandleSize((GSHandle)memo.params) / sizeof(API_AddParType));
    bool anyChanged = false;

    for (UInt32 i = 0; i < nParams; ++i) {
        API_AddParType& par = (*memo.params)[i];
        if (par.typeMod != API_ParSimple)
            continue;

        lua_getfield(L, 2, par.name);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        switch (par.typeID) {
            case APIParT_Integer:
            case APIParT_Length:
            case APIParT_Angle:
            case APIParT_RealNum:
            case APIParT_Intens:
                if (lua_isnumber(L, -1)) {
                    par.value.real = lua_tonumber(L, -1);
                    anyChanged = true;
                }
                break;
            case APIParT_LightSw:
            case APIParT_Boolean:
                par.value.real = lua_toboolean(L, -1) ? 1.0 : 0.0;
                anyChanged = true;
                break;
            case APIParT_CString:
            {
                const char* s = lua_tostring(L, -1);
                if (s) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
                    if (wlen > 0 && wlen <= API_UAddParStrLen) {
                        MultiByteToWideChar(CP_UTF8, 0, s, -1, (LPWCH)par.value.uStr, wlen);
                        anyChanged = true;
                    }
                }
                break;
            }
            default:
                break;
        }
        lua_pop(L, 1);
    }

    if (!anyChanged) {
        ACAPI_DisposeElemMemoHdls(&memo);
        lua_pushboolean(L, true);
        return 1;
    }

    API_Element mask;
    ACAPI_ELEMENT_MASK_CLEAR(mask);

    err = ACAPI_CallUndoableCommand("Modify Parameters", [&]() -> GSErrCode {
        return ACAPI_Element_Change(&elem, &mask, &memo, APIMemoMask_AddPars, true);
    });

    ACAPI_DisposeElemMemoHdls(&memo);

    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "setparams failed: err=%d", (int)err);
        return 2;
    }

    lua_pushboolean(L, true);
    return 1;
}

static int FindObject(lua_State* L)
{
    const char* name = lua_tostring(L, 1);
    if (!name) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a name string argument");
        return 2;
    }

    API_LibPart libPart;
    BNZeroMemory(&libPart, sizeof(libPart));
    GS::ucscpy(libPart.docu_UName, GS::UniString(name).ToUStr());

    GSErrCode err = ACAPI_LibraryPart_Search(&libPart, false);
    if (err != NoError || libPart.index == 0) {
        lua_pushnil(L);
        lua_pushfstring(L, "library part not found: %s", name);
        return 2;
    }

    lua_pushinteger(L, libPart.index);
    lua_pushstring(L, GS::UniString(libPart.docu_UName).ToCStr().Get());
    return 2;
}

static int CreateElement(lua_State* L)
{
    if (!lua_isinteger(L, 1) || !lua_istable(L, 2)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected libInd (integer) and position table arguments");
        return 2;
    }

    Int32 libInd = (Int32)lua_tointeger(L, 1);

    // Position: { x, y } or { x=, y= }
    double posX = 0, posY = 0;
    lua_getfield(L, 2, "x");
    if (lua_isnumber(L, -1)) {
        posX = lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 2, "y");
        if (lua_isnumber(L, -1)) {
            posY = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
    } else {
        lua_pop(L, 1);
        lua_rawgeti(L, 2, 1);
        if (lua_isnumber(L, -1)) {
            posX = lua_tonumber(L, -1);
            lua_pop(L, 1);
            lua_rawgeti(L, 2, 2);
            if (lua_isnumber(L, -1)) {
                posY = lua_tonumber(L, -1);
            }
            lua_pop(L, 1);
        } else {
            lua_pop(L, 1);
        }
    }

    // Optional params table (3rd arg)
    bool hasParams = lua_istable(L, 3);

    API_Guid createdGuid = APINULLGuid;
    char errorMsg[256];

    GSErrCode err = ACAPI_CallUndoableCommand("Create Element", [&]() -> GSErrCode {
        API_Element elem;
        BNZeroMemory(&elem, sizeof(elem));

        // Set header type from libInd
        API_LibPart libPart;
        BNZeroMemory(&libPart, sizeof(libPart));
        libPart.index = libInd;
        GSErrCode e = ACAPI_LibraryPart_Get(&libPart);
        if (e != NoError) { std::sprintf(errorMsg, "LibraryPart_Get failed: err=%d", (int)e); return e; }

        // Determine element type from the library part
        elem.header.type.typeID = static_cast<API_ElemTypeID>(libPart.typeID);

        // Set common fields
        elem.header.floorInd = 1; // default floor

        // Set position based on type
        switch (libPart.typeID) {
            case API_ObjectID:
                elem.object.libInd = libInd;
                elem.object.pos.x = posX;
                elem.object.pos.y = posY;
                break;
            default:
                std::sprintf(errorMsg, "unsupported element type for creation");
                return APIERR_GENERAL;
        }

        // Get default params from the library part and apply overrides
        API_ElementMemo memo;
        BNZeroMemory(&memo, sizeof(memo));

        if (hasParams) {
            API_ParamOwnerType paramOwner;
            BNZeroMemory(&paramOwner, sizeof(paramOwner));
            paramOwner.libInd = libInd;
            e = ACAPI_LibraryPart_OpenParameters(&paramOwner);
            if (e != NoError) {
                std::sprintf(errorMsg, "OpenParameters failed: err=%d", (int)e);
                return e;
            }

            API_GetParamsType getParams;
            BNZeroMemory(&getParams, sizeof(getParams));
            e = ACAPI_LibraryPart_GetActParameters(&getParams);
            if (e != NoError || getParams.params == nullptr) {
                ACAPI_LibraryPart_CloseParameters();
                std::sprintf(errorMsg, "GetActParameters failed: err=%d", (int)e);
                return e != NoError ? e : APIERR_GENERAL;
            }

            UInt32 nParams = (UInt32)(BMGetHandleSize((GSHandle)getParams.params) / sizeof(API_AddParType));
            for (UInt32 i = 0; i < nParams; ++i) {
                API_AddParType& par = (*getParams.params)[i];
                if (par.typeMod != API_ParSimple)
                    continue;

                lua_getfield(L, 3, par.name);
                if (lua_isnil(L, -1)) { lua_pop(L, 1); continue; }

                switch (par.typeID) {
                    case APIParT_Integer: case APIParT_Length:
                    case APIParT_Angle: case APIParT_RealNum: case APIParT_Intens:
                        if (lua_isnumber(L, -1))
                            par.value.real = lua_tonumber(L, -1);
                        break;
                    case APIParT_LightSw: case APIParT_Boolean:
                        par.value.real = lua_toboolean(L, -1) ? 1.0 : 0.0;
                        break;
                    case APIParT_CString: {
                        const char* s = lua_tostring(L, -1);
                        if (s) {
                            int wlen = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
                            if (wlen > 0 && wlen <= API_UAddParStrLen) {
                                MultiByteToWideChar(CP_UTF8, 0, s, -1, (LPWCH)par.value.uStr, wlen);
                            }
                        }
                        break;
                    }
                    default: break;
                }
                lua_pop(L, 1);
            }

            // Transfer params handle to memo
            ACAPI_DisposeAddParHdl(&memo.params);
            memo.params = getParams.params;
            ACAPI_LibraryPart_CloseParameters();
        }

        // Create the element
        e = ACAPI_Element_Create(&elem, hasParams ? &memo : nullptr);
        if (e != NoError) {
            if (hasParams) ACAPI_DisposeElemMemoHdls(&memo);
            std::sprintf(errorMsg, "Element_Create failed: err=%d", (int)e);
            return e;
        }

        if (hasParams) ACAPI_DisposeElemMemoHdls(&memo);

        createdGuid = elem.header.guid;
        return NoError;
    });

    if (err != NoError || createdGuid == APINULLGuid) {
        lua_pushnil(L);
        lua_pushstring(L, errorMsg);
        return 2;
    }

    GS::UniString guidStr = APIGuidToString(createdGuid);
    lua_pushstring(L, guidStr.ToCStr().Get());
    return 1;
}

static int AddWall(lua_State* L)
{
    if (!lua_istable(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a table argument");
        return 2;
    }

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.type.typeID = API_WallID;

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));
    bool hasMemo = false;

    // Check for polygonal wall
    lua_getfield(L, 1, "poly");
    if (lua_istable(L, -1)) {
        elem.wall.type = APIWtyp_Poly;

        Int32 nVerts = (Int32)lua_rawlen(L, -1);
        if (nVerts < 3) {
            lua_pop(L, 1);
            lua_pushnil(L);
            lua_pushstring(L, "poly needs at least 3 vertices");
            return 2;
        }

        memo.coords = (API_Coord**)BMAllocateHandle((nVerts + 1) * sizeof(API_Coord), ALLOCATE_CLEAR, 0);
        if (memo.coords == nullptr) {
            lua_pop(L, 1);
            lua_pushnil(L);
            lua_pushstring(L, "out of memory");
            return 2;
        }
        hasMemo = true;

        for (Int32 i = 0; i < nVerts; i++) {
            lua_rawgeti(L, -1, i + 1);
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "x");
                (*memo.coords)[i].x = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
                lua_pop(L, 1);
                lua_getfield(L, -1, "y");
                (*memo.coords)[i].y = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        // Close polygon: last vertex = first
        (*memo.coords)[nVerts] = (*memo.coords)[0];
    }
    lua_pop(L, 1);

    // Read straight wall position only if not polygonal
    if (elem.wall.type != APIWtyp_Poly) {
        lua_getfield(L, 1, "begC");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "x");  elem.wall.begC.x = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0; lua_pop(L, 1);
            lua_getfield(L, -1, "y");  elem.wall.begC.y = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0; lua_pop(L, 1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "endC");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "x");  elem.wall.endC.x = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 5; lua_pop(L, 1);
            lua_getfield(L, -1, "y");  elem.wall.endC.y = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0; lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    lua_getfield(L, 1, "height");
    elem.wall.height = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 3.0;
    lua_pop(L, 1);

    lua_getfield(L, 1, "thickness");
    elem.wall.thickness = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.25;
    lua_pop(L, 1);

    lua_getfield(L, 1, "layer");
    if (lua_isinteger(L, -1))
        elem.header.layer = ACAPI_CreateAttributeIndex((Int32)lua_tointeger(L, -1));
    lua_pop(L, 1);

    // floor (matches ArchiCAD's naming); try "floor" first, fall back to "storey"
    lua_getfield(L, 1, "floor");
    if (!lua_isinteger(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "storey");
    }
    elem.header.floorInd = (short)(lua_isinteger(L, -1) ? lua_tointeger(L, -1) : 1);
    lua_pop(L, 1);

    API_Guid createdGuid = APINULLGuid;
    char errorMsg[256];
    GSErrCode err = ACAPI_CallUndoableCommand("Create Wall", [&]() -> GSErrCode {
        GSErrCode e = ACAPI_Element_Create(&elem, hasMemo ? &memo : nullptr);
        if (e != NoError) {
            std::sprintf(errorMsg, "addWall failed: err=%d", (int)e);
            return e;
        }
        createdGuid = elem.header.guid;
        return NoError;
    });

    if (hasMemo)
        ACAPI_DisposeElemMemoHdls(&memo);

    if (err != NoError || createdGuid == APINULLGuid) {
        lua_pushnil(L);
        lua_pushstring(L, errorMsg);
        return 2;
    }

    GS::UniString guidStr = APIGuidToString(createdGuid);
    lua_pushstring(L, guidStr.ToCStr().Get());
    return 1;
}

static int SetGDLParam(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    const char* paramName = lua_tostring(L, 2);
    if (!guidStr || !paramName || !(lua_isnumber(L, 3) || lua_isstring(L, 3))) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "expected guid, paramName, value");
        return 2;
    }
    bool isString = (lua_type(L, 3) == LUA_TSTRING);
    double value = 0.0;
    const char* valueStr = nullptr;
    if (isString)
        valueStr = lua_tostring(L, 3);
    else
        value = lua_tonumber(L, 3);

    API_Guid guid = APIGuidFromString(guidStr);

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get(&elem);
    if (err != NoError) {
        lua_pushboolean(L, false);
        lua_pushfstring(L, "element not found: err=%d", (int)err);
        return 2;
    }

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));
    err = ACAPI_Element_GetMemo(guid, &memo, APIMemoMask_AddPars);
    if (err != NoError || !memo.params) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "no params memo");
        return 2;
    }

    UInt32 n = (UInt32)(BMGetHandleSize((GSHandle)memo.params) / sizeof(API_AddParType));
    bool found = false;
    for (UInt32 i = 0; i < n; ++i) {
        API_AddParType& par = (*memo.params)[i];
        if (par.typeMod != API_ParSimple)
            continue;
        if (GS::UniString(par.name) == paramName) {
            if (isString) {
                GS::UniString unistr(valueStr);
                UInt32 len = unistr.GetLength();
                if (len >= API_UAddParStrLen)
                    len = API_UAddParStrLen - 1;
                for (UInt32 k = 0; k < len; ++k)
                    par.value.uStr[k] = unistr[k];
                par.value.uStr[len] = 0;
            } else {
                switch (par.typeID) {
                    case APIParT_Angle:
                        par.value.real = value * 3.14159265358979323846 / 180.0;
                        break;
                    case APIParT_Boolean:
                        par.value.real = value != 0.0 ? 1.0 : 0.0;
                        break;
                    default:
                        par.value.real = value;
                        break;
                }
            }
            found = true;
            break;
        }
    }

    if (!found) {
        ACAPI_DisposeElemMemoHdls(&memo);
        lua_pushboolean(L, false);
        lua_pushfstring(L, "param '%s' not found or not modifiable", paramName);
        return 2;
    }

    API_Element mask;
    ACAPI_ELEMENT_MASK_CLEAR(mask);

    err = ACAPI_CallUndoableCommand("Set GDL Param", [&]() -> GSErrCode {
        return ACAPI_Element_Change(&elem, &mask, &memo, APIMemoMask_AddPars, true);
    });
    ACAPI_DisposeElemMemoHdls(&memo);

    if (err != NoError) {
        lua_pushboolean(L, false);
        lua_pushfstring(L, "change failed: err=%d", (int)err);
        return 2;
    }

    lua_pushboolean(L, true);
    return 1;
}

static int AddWindow(lua_State* L)
{
    const char* wallGuidStr = lua_tostring(L, 1);
    if (!wallGuidStr || !lua_istable(L, 2)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a wall GUID and a params table");
        return 2;
    }

    API_Guid wallGuid = APIGuidFromString(wallGuidStr);

    // Verify wall exists and is straight
    API_Element wallElem;
    BNZeroMemory(&wallElem, sizeof(wallElem));
    wallElem.header.guid = wallGuid;
    GSErrCode err = ACAPI_Element_Get(&wallElem);
    if (err != NoError || wallElem.header.type.typeID != API_WallID) {
        lua_pushnil(L);
        lua_pushstring(L, "wall not found");
        return 2;
    }
    if (wallElem.wall.type == APIWtyp_Poly) {
        lua_pushnil(L);
        lua_pushstring(L, "cannot place window in polygonal wall");
        return 2;
    }

    // Read params
    lua_getfield(L, 2, "objLoc");
    double objLoc = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 2.0;
    lua_pop(L, 1);

    lua_getfield(L, 2, "height");
    double height = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.5;
    lua_pop(L, 1);

    lua_getfield(L, 2, "width");
    double width = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0;
    lua_pop(L, 1);

    lua_getfield(L, 2, "sillHeight");
    double sillHeight = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.9;
    lua_pop(L, 1);

    // Read wallSide / mirrored / openingAngle params before any element setup
    lua_getfield(L, 2, "wallSide");
    bool wallSideInside = false;
    if (lua_isstring(L, -1)) {
        const char* s = lua_tostring(L, -1);
        wallSideInside = (GS::UniString(s).Compare("inside") == 0);
    } else if (lua_isinteger(L, -1)) {
        wallSideInside = (lua_tointeger(L, -1) != 0);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "mirrored");
    bool mirrored = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "openingAngle");
    double openingAngle = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 45.0;
    lua_pop(L, 1);

    // Set up window element
    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.type.typeID = API_WindowID;

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));

    // Get defaults for window
    err = ACAPI_Element_GetDefaults(&elem, &memo);
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "GetDefaults failed: err=%d", (int)err);
        return 2;
    }

    // Position on wall + set opening size/mirror before Create
    elem.window.owner = wallGuid;
    elem.window.objLoc = objLoc;
    elem.window.openingBase.height = height;
    elem.window.openingBase.width = width;
    elem.window.lower = sillHeight;
    elem.window.openingBase.reflected = mirrored;

    // Create
    API_Guid createdGuid = APINULLGuid;

    err = ACAPI_CallUndoableCommand("Add Window", [&]() -> GSErrCode {
        GSErrCode e = ACAPI_Element_Create(&elem, &memo);
        if (e == NoError)
            createdGuid = elem.header.guid;
        return e;
    });

    ACAPI_DisposeElemMemoHdls(&memo);

    if (err != NoError || createdGuid == APINULLGuid) {
        lua_pushnil(L);
        lua_pushfstring(L, "create window failed: err=%d", (int)err);
        return 2;
    }

    // Post-create: mirror only (wallSide handled by Lua setGDLParam)
    ACAPI_CallUndoableCommand("Post-create Window", [&]() -> GSErrCode {
        API_Element elem2;
        BNZeroMemory(&elem2, sizeof(elem2));
        elem2.header.guid = createdGuid;
        ACAPI_Element_Get(&elem2);
        API_Element mask2;
        ACAPI_ELEMENT_MASK_CLEAR(mask2);
        elem2.window.openingBase.reflected = mirrored;
        ACAPI_ELEMENT_MASK_SET(mask2, API_WindowType, openingBase.reflected);
        return ACAPI_Element_Change(&elem2, &mask2, nullptr, 0, true);
    });

    GS::UniString guidStr = APIGuidToString(createdGuid);
    lua_pushstring(L, guidStr.ToCStr().Get());
    return 1;
}

static int AddDoor(lua_State* L)
{
    const char* wallGuidStr = lua_tostring(L, 1);
    if (!wallGuidStr || !lua_istable(L, 2)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a wall GUID and a params table");
        return 2;
    }

    API_Guid wallGuid = APIGuidFromString(wallGuidStr);

    // Verify wall exists and is straight
    API_Element wallElem;
    BNZeroMemory(&wallElem, sizeof(wallElem));
    wallElem.header.guid = wallGuid;
    GSErrCode err = ACAPI_Element_Get(&wallElem);
    if (err != NoError || wallElem.header.type.typeID != API_WallID) {
        lua_pushnil(L);
        lua_pushstring(L, "wall not found");
        return 2;
    }
    if (wallElem.wall.type == APIWtyp_Poly) {
        lua_pushnil(L);
        lua_pushstring(L, "cannot place door in polygonal wall");
        return 2;
    }

    // Read params
    lua_getfield(L, 2, "objLoc");
    double objLoc = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 2.0;
    lua_pop(L, 1);

    lua_getfield(L, 2, "height");
    double height = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 2.0;
    lua_pop(L, 1);

    lua_getfield(L, 2, "width");
    double width = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.9;
    lua_pop(L, 1);

    lua_getfield(L, 2, "wallSide");
    bool wallSideInside = false;
    if (lua_isstring(L, -1)) {
        const char* s = lua_tostring(L, -1);
        wallSideInside = (GS::UniString(s).Compare("inside") == 0);
    } else if (lua_isinteger(L, -1)) {
        wallSideInside = (lua_tointeger(L, -1) != 0);
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "mirrored");
    bool mirrored = lua_toboolean(L, -1);
    lua_pop(L, 1);

    // Set up door element
    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.type.typeID = API_DoorID;

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));

    API_SubElement marker;
    BNZeroMemory(&marker, sizeof(marker));
    marker.subType = APISubElement_MainMarker;

    // Get defaults for door
    err = ACAPI_Element_GetDefaultsExt(&elem, &memo, 1UL, &marker);
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "GetDefaultsExt failed: err=%d", (int)err);
        return 2;
    }

    // Get marker parent library part
    API_LibPart libPart;
    BNZeroMemory(&libPart, sizeof(libPart));
    err = ACAPI_LibraryPart_GetMarkerParent(elem.header.type, libPart);
    if (err != NoError) {
        ACAPI_DisposeElemMemoHdls(&memo);
        ACAPI_DisposeElemMemoHdls(&marker.memo);
        lua_pushnil(L);
        lua_pushfstring(L, "GetMarkerParent failed: err=%d", (int)err);
        return 2;
    }

    err = ACAPI_LibraryPart_Search(&libPart, false, true);
    if (err != NoError) {
        ACAPI_DisposeElemMemoHdls(&memo);
        ACAPI_DisposeElemMemoHdls(&marker.memo);
        lua_pushnil(L);
        lua_pushfstring(L, "LibraryPart_Search failed: err=%d", (int)err);
        return 2;
    }
    delete libPart.location;

    // Get marker params
    double a, b;
    Int32 addParNum;
    API_AddParType** markAddPars = nullptr;
    err = ACAPI_LibraryPart_GetParams(libPart.index, &a, &b, &addParNum, &markAddPars);
    if (err != NoError) {
        ACAPI_DisposeElemMemoHdls(&memo);
        ACAPI_DisposeElemMemoHdls(&marker.memo);
        lua_pushnil(L);
        lua_pushfstring(L, "GetParams failed: err=%d", (int)err);
        return 2;
    }

    marker.memo.params = markAddPars;
    marker.subElem.object.pen = 166;
    marker.subElem.object.useObjPens = true;

    // Position on wall
    elem.door.owner = wallGuid;
    elem.door.objLoc = objLoc;
    elem.door.openingBase.height = height;
    elem.door.openingBase.width = width;
    // NOTE: wallSide/reflected not in AC27 API_DoorType — use GDL params instead

    // Create
    char errorMsg[256];
    API_Guid createdGuid = APINULLGuid;

    err = ACAPI_CallUndoableCommand("Add Door", [&]() -> GSErrCode {
        GSErrCode e = ACAPI_Element_CreateExt(&elem, &memo, 1UL, &marker);
        if (e != NoError) {
            std::sprintf(errorMsg, "create door failed: err=%d", (int)e);
            return e;
        }
        createdGuid = elem.header.guid;
        return NoError;
    });

    ACAPI_DisposeElemMemoHdls(&memo);
    ACAPI_DisposeElemMemoHdls(&marker.memo);

    if (err != NoError || createdGuid == APINULLGuid) {
        lua_pushnil(L);
        lua_pushstring(L, errorMsg);
        return 2;
    }

    GS::UniString guidStr = APIGuidToString(createdGuid);
    lua_pushstring(L, guidStr.ToCStr().Get());
    return 1;
}

static int AddSlab(lua_State* L)
{
    if (!lua_istable(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a table argument");
        return 2;
    }

    // Read poly
    lua_getfield(L, 1, "poly");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, "poly table required");
        return 2;
    }

    Int32 nVerts = (Int32)lua_rawlen(L, -1);
    if (nVerts < 3) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, "poly needs at least 3 vertices");
        return 2;
    }

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.type.typeID = API_SlabID;

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));

    memo.coords = (API_Coord**)BMAllocateHandle((nVerts + 1) * sizeof(API_Coord), ALLOCATE_CLEAR, 0);
    if (memo.coords == nullptr) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, "out of memory");
        return 2;
    }

    for (Int32 i = 0; i < nVerts; i++) {
        lua_rawgeti(L, -1, i + 1);
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "x");
            (*memo.coords)[i].x = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
            lua_pop(L, 1);
            lua_getfield(L, -1, "y");
            (*memo.coords)[i].y = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    (*memo.coords)[nVerts] = (*memo.coords)[0];
    lua_pop(L, 1); // pop poly table

    // Read params
    lua_getfield(L, 1, "thickness");
    elem.slab.thickness = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.2;
    lua_pop(L, 1);

    lua_getfield(L, 1, "layer");
    if (lua_isinteger(L, -1))
        elem.header.layer = ACAPI_CreateAttributeIndex((Int32)lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "floor");
    if (!lua_isinteger(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "storey");
    }
    elem.header.floorInd = (short)(lua_isinteger(L, -1) ? lua_tointeger(L, -1) : 1);
    lua_pop(L, 1);

    API_Guid createdGuid = APINULLGuid;
    char errorMsg[256];
    GSErrCode err = ACAPI_CallUndoableCommand("Create Slab", [&]() -> GSErrCode {
        GSErrCode e = ACAPI_Element_Create(&elem, &memo);
        if (e != NoError) {
            std::sprintf(errorMsg, "addSlab failed: err=%d", (int)e);
            return e;
        }
        createdGuid = elem.header.guid;
        return NoError;
    });

    ACAPI_DisposeElemMemoHdls(&memo);

    if (err != NoError || createdGuid == APINULLGuid) {
        lua_pushnil(L);
        lua_pushstring(L, errorMsg);
        return 2;
    }

    GS::UniString guidStr = APIGuidToString(createdGuid);
    lua_pushstring(L, guidStr.ToCStr().Get());
    return 1;
}

static int AddRoof(lua_State* L)
{
    if (!lua_istable(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a table argument");
        return 2;
    }

    // Read poly
    lua_getfield(L, 1, "poly");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, "poly table required");
        return 2;
    }

    Int32 nVerts = (Int32)lua_rawlen(L, -1);
    if (nVerts < 3) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, "poly needs at least 3 vertices");
        return 2;
    }

    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.type.typeID = API_RoofID;

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));

    memo.coords = (API_Coord**)BMAllocateHandle((nVerts + 1) * sizeof(API_Coord), ALLOCATE_CLEAR, 0);
    if (memo.coords == nullptr) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, "out of memory");
        return 2;
    }

    for (Int32 i = 0; i < nVerts; i++) {
        lua_rawgeti(L, -1, i + 1);
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "x");
            (*memo.coords)[i].x = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
            lua_pop(L, 1);
            lua_getfield(L, -1, "y");
            (*memo.coords)[i].y = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0;
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    (*memo.coords)[nVerts] = (*memo.coords)[0];
    lua_pop(L, 1); // pop poly table

    // Read params
    lua_getfield(L, 1, "thickness");
    // NOTE: thickness is not a direct field in AC27 API_RoofType
    // It should be set via shellBase or GDL parameters.
    // Lua script passes it but it's consumed here to keep stack balanced:
    lua_pop(L, 1);

    lua_getfield(L, 1, "layer");
    if (lua_isinteger(L, -1))
        elem.header.layer = ACAPI_CreateAttributeIndex((Int32)lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "floor");
    if (!lua_isinteger(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "storey");
    }
    elem.header.floorInd = (short)(lua_isinteger(L, -1) ? lua_tointeger(L, -1) : 1);
    lua_pop(L, 1);

    API_Guid createdGuid = APINULLGuid;
    char errorMsg[256];
    GSErrCode err = ACAPI_CallUndoableCommand("Create Roof", [&]() -> GSErrCode {
        GSErrCode e = ACAPI_Element_Create(&elem, &memo);
        if (e != NoError) {
            std::sprintf(errorMsg, "addRoof failed: err=%d", (int)e);
            return e;
        }
        createdGuid = elem.header.guid;
        return NoError;
    });

    ACAPI_DisposeElemMemoHdls(&memo);

    if (err != NoError || createdGuid == APINULLGuid) {
        lua_pushnil(L);
        lua_pushstring(L, errorMsg);
        return 2;
    }

    GS::UniString guidStr = APIGuidToString(createdGuid);
    lua_pushstring(L, guidStr.ToCStr().Get());
    return 1;
}

static int GetCurrentFloor(lua_State* L)
{
    API_StoryInfo storyInfo;
    BNZeroMemory(&storyInfo, sizeof(storyInfo));
    GSErrCode err = ACAPI_ProjectSetting_GetStorySettings(&storyInfo);
    if (err != NoError) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushinteger(L, storyInfo.actStory);

    if (storyInfo.data != nullptr)
        BMKillHandle((GSHandle*)&storyInfo.data);

    return 1;
}

inline void Register(lua_State* L)
{
    lua_newtable(L);

    lua_pushcfunction(L, GetSelection);
    lua_setfield(L, -2, "getSel");

    lua_pushcfunction(L, GetWall);
    lua_setfield(L, -2, "getWall");

    lua_pushcfunction(L, GetElement);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, GetCurrentFloor);
    lua_setfield(L, -2, "getCurrentFloor");

    lua_pushcfunction(L, GetPoly);
    lua_setfield(L, -2, "getPoly");

    lua_pushcfunction(L, GetParams);
    lua_setfield(L, -2, "getParams");

    lua_pushcfunction(L, BeginUndo);
    lua_setfield(L, -2, "beginUndo");

    lua_pushcfunction(L, EndUndo);
    lua_setfield(L, -2, "endUndo");

    lua_pushcfunction(L, SetWall);
    lua_setfield(L, -2, "setWall");

    lua_pushcfunction(L, SetElement);
    lua_setfield(L, -2, "set");

    lua_pushcfunction(L, SetParams);
    lua_setfield(L, -2, "setParams");

    lua_pushcfunction(L, FindObject);
    lua_setfield(L, -2, "findObject");

    lua_pushcfunction(L, CreateElement);
    lua_setfield(L, -2, "create");

    lua_pushcfunction(L, AddWall);
    lua_setfield(L, -2, "addWall");

    lua_pushcfunction(L, AddWindow);
    lua_setfield(L, -2, "addWindow");

    lua_pushcfunction(L, AddDoor);
    lua_setfield(L, -2, "addDoor");

    lua_pushcfunction(L, AddSlab);
    lua_setfield(L, -2, "addSlab");

    lua_pushcfunction(L, AddRoof);
    lua_setfield(L, -2, "addRoof");

    lua_pushcfunction(L, SetGDLParam);
    lua_setfield(L, -2, "setGDLParam");

    lua_setglobal(L, "acapi");
}

} // namespace APIModule
} // namespace ArchiLua