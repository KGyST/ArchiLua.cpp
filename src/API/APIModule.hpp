#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "ACAPinc.h"
#include "../ArchiLua.hpp"
#include "../Bridge/LuaDebugger.hpp"

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

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
        } else if (strcmp(key, "pos") == 0 && lua_istable(L, -1)) {
            if (elem.header.type.typeID != API_ObjectID) {
                lua_pop(L, 1);
                lua_pushnil(L);
                lua_pushstring(L, "set: pos is only supported for objects");
                return 2;
            }
            lua_getfield(L, -1, "x");
            lua_getfield(L, -2, "y");
            if (lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
                elem.object.pos.x = lua_tonumber(L, -2);
                elem.object.pos.y = lua_tonumber(L, -1);
                ACAPI_ELEMENT_MASK_SET(mask, API_ObjectType, pos);
            }
            lua_pop(L, 2);
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

static int DeleteElement(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (guidStr == nullptr) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "delete: expected a GUID string");
        return 2;
    }

    API_Guid guid = APIGuidFromString(guidStr);

    if (UndoIsActive()) {
        UndoBufferDelete(guid); // replayed with the batch; errors surface via endUndo
        lua_pushboolean(L, true);
        return 1;
    }

    GS::Array<API_Guid> guids;
    guids.Push(guid);

    GSErrCode err = ACAPI_CallUndoableCommand("Delete Element", [&]() -> GSErrCode {
        return ACAPI_Element_Delete(guids);
    });
    if (err != NoError) {
        lua_pushboolean(L, false);
        lua_pushfstring(L, "delete failed: err=%d", (int)err);
        return 2;
    }

    lua_pushboolean(L, true);
    return 1;
}

// Applies a Lua params table (at paramsIdx) onto a params memo.
// Returns the number of parameters actually written.
static int ApplyParamOverrides(lua_State* L, int paramsIdx, API_ElementMemo& memo)
{
    int applied = 0;
    if (memo.params == nullptr)
        return 0;
    UInt32 nParams = (UInt32)(BMGetHandleSize((GSHandle)memo.params) / sizeof(API_AddParType));
    for (UInt32 i = 0; i < nParams; ++i) {
        API_AddParType& par = (*memo.params)[i];
        if (par.typeMod != API_ParSimple)
            continue;
        lua_getfield(L, paramsIdx, par.name);
        if (lua_isnil(L, -1)) { lua_pop(L, 1); continue; }
        bool wrote = false;
        switch (par.typeID) {
            case APIParT_Integer: case APIParT_Length:
            case APIParT_Angle: case APIParT_RealNum: case APIParT_Intens:
                if (lua_isnumber(L, -1)) {
                    par.value.real = lua_tonumber(L, -1);
                    wrote = true;
                }
                break;
            case APIParT_LightSw: case APIParT_Boolean:
                par.value.real = lua_toboolean(L, -1) ? 1.0 : 0.0;
                wrote = true;
                break;
            case APIParT_CString: {
                const char* s = lua_tostring(L, -1);
                if (s) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
                    if (wlen > 0 && wlen <= API_UAddParStrLen) {
                        MultiByteToWideChar(CP_UTF8, 0, s, -1, (LPWCH)par.value.uStr, wlen);
                        wrote = true;
                    }
                }
                break;
            }
            default: break;
        }
        if (wrote)
            ++applied;
        lua_pop(L, 1);
    }
    return applied;
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

    bool anyChanged = ApplyParamOverrides(L, 2, memo) > 0;

    if (!anyChanged) {
        ACAPI_DisposeElemMemoHdls(&memo);
        lua_pushboolean(L, true);
        return 1;
    }

    API_Element mask;
    ACAPI_ELEMENT_MASK_CLEAR(mask);

    if (UndoIsActive()) {
        UndoBufferMemo(guid, elem, mask, memo); // buffer owns the memo now
    } else {
        err = ACAPI_CallUndoableCommand("Modify Parameters", [&]() -> GSErrCode {
            return ACAPI_Element_Change(&elem, &mask, &memo, APIMemoMask_AddPars, true);
        });

        ACAPI_DisposeElemMemoHdls(&memo);

        if (err != NoError) {
            lua_pushnil(L);
            lua_pushfstring(L, "setparams failed: err=%d", (int)err);
            return 2;
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

static bool UniAsciiCaseEqual(const GS::UniString& a, const char* b)
{
    if (b == nullptr)
        return false;
    std::string au(a.ToCStr().Get());
    if (au.size() != strlen(b))
        return false;
    for (size_t i = 0; i < au.size(); ++i) {
        char ca = au[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += (char)('a' - 'A');
        if (cb >= 'A' && cb <= 'Z') cb += (char)('a' - 'A');
        if (ca != cb)
            return false;
    }
    return true;
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

    // Never trust the index blindly: a non-matching part here means silent
    // wrong placements downstream (wrong geometry, skipped param overrides).
    GS::UniString found(libPart.docu_UName);
    if (!UniAsciiCaseEqual(found, name)) {
        lua_pushnil(L);
        lua_pushfstring(L, "library part mismatch: asked '%s', found '%s'", name, found.ToCStr().Get());
        return 2;
    }

    lua_pushinteger(L, libPart.index);
    lua_pushstring(L, found.ToCStr().Get());
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

    // Position: { x, y } or { x=, y= }, plus optional angle= (radians)
    double posX = 0, posY = 0, posAngle = 0;
    bool hasAngle = false;
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

    lua_getfield(L, 2, "angle");
    if (lua_isnumber(L, -1)) {
        posAngle = lua_tonumber(L, -1);
        hasAngle = true;
    }
    lua_pop(L, 1);

    // Optional params table (3rd arg)
    bool hasParams = lua_istable(L, 3);

    API_Guid createdGuid = APINULLGuid;
    char errorMsg[256];
    int appliedOverrides = 0;

    GSErrCode err = ACAPI_CallUndoableCommand("Create Element", [&]() -> GSErrCode {
        API_Element elem;
        BNZeroMemory(&elem, sizeof(elem));

        // Set header type from libInd
        API_LibPart libPart;
        BNZeroMemory(&libPart, sizeof(libPart));
        libPart.index = libInd;
        GSErrCode e = ACAPI_LibraryPart_Get(&libPart);
        if (e != NoError) { std::sprintf(errorMsg, "LibraryPart_Get failed: err=%d", (int)e); return e; }

        // Determine element type from the library part.
        // NOTE: libPart.typeID is API_LibTypeID (Object=4), NOT API_ElemTypeID
        // (Object=6) — map explicitly, never cast.
        switch (libPart.typeID) {
            case APILib_ObjectID:
                elem.header.type.typeID = API_ObjectID;
                elem.object.libInd = libInd; // must precede GetDefaults (params resolve from it)
                break;
            default:
                std::sprintf(errorMsg, "unsupported library part type for creation (libType=%d)", (int)libPart.typeID);
                return APIERR_GENERAL;
        }

        // Documented pattern: defaults (layer, etc.) come from GetDefaults,
        // geometry/position is filled in by us. A zeroed header (layer 0)
        // makes Element_Create fail with APIERR_BADINDEX.
        API_ElementMemo memo;
        BNZeroMemory(&memo, sizeof(memo));
        e = ACAPI_Element_GetDefaults(&elem, &memo);
        if (e != NoError) {
            std::sprintf(errorMsg, "GetDefaults failed: err=%d", (int)e);
            return e;
        }

        elem.object.pos.x = posX;
        elem.object.pos.y = posY;
        if (hasAngle)
            elem.object.angle = posAngle;

        // NOTE: overrides are applied post-create via Change (Create ignores
        // memo params for objects). The memo here carries defaults only.

        // Re-assert libInd: GetDefaults may reset it to the default part.
        elem.object.libInd = libInd;

        // Create the element (defaults memo; Create may ignore memo params,
        // so overrides are applied via Change below — the proven path).
        e = ACAPI_Element_Create(&elem, &memo);
        ACAPI_DisposeElemMemoHdls(&memo);
        if (e != NoError) {
            std::sprintf(errorMsg, "Element_Create failed: err=%d", (int)e);
            return e;
        }
        createdGuid = elem.header.guid;

        // Verify the placed element really is our part (GetDefaults is known
        // to reset libInd to the default part on some flows).
        {
            API_Element placedElem;
            BNZeroMemory(&placedElem, sizeof(placedElem));
            placedElem.header.guid = createdGuid;
            e = ACAPI_Element_Get(&placedElem);
            if (e == NoError && placedElem.header.type.typeID == API_ObjectID &&
                placedElem.object.libInd != libInd) {
                std::sprintf(errorMsg, "placed wrong part: wanted libInd %d, got %d",
                             (int)libInd, (int)placedElem.object.libInd);
                return APIERR_GENERAL;
            }
        }

        if (hasParams) {
            API_Element elem2;
            BNZeroMemory(&elem2, sizeof(elem2));
            elem2.header.guid = createdGuid;
            e = ACAPI_Element_Get(&elem2);
            if (e != NoError) {
                std::sprintf(errorMsg, "post-create Get failed: err=%d", (int)e);
                return e;
            }
            API_ElementMemo memo2;
            BNZeroMemory(&memo2, sizeof(memo2));
            e = ACAPI_Element_GetMemo(createdGuid, &memo2, APIMemoMask_AddPars);
            if (e != NoError || memo2.params == nullptr) {
                if (e == NoError)
                    ACAPI_DisposeElemMemoHdls(&memo2);
                std::sprintf(errorMsg, "create: no params memo (overrides refused)");
                return APIERR_GENERAL;
            }
            appliedOverrides = ApplyParamOverrides(L, 3, memo2);
            API_Element mask2;
            ACAPI_ELEMENT_MASK_CLEAR(mask2);
            e = ACAPI_Element_Change(&elem2, &mask2, &memo2, APIMemoMask_AddPars, true);
            ACAPI_DisposeElemMemoHdls(&memo2);
            if (e != NoError) {
                std::sprintf(errorMsg, "post-create param change failed: err=%d", (int)e);
                return e;
            }
            if (appliedOverrides == 0) {
                ACAPI_WriteReport("create: warning — none of the override names matched a parameter", false);
            }
        }
        return NoError;
    });

    if (err != NoError || createdGuid == APINULLGuid) {
        lua_pushnil(L);
        lua_pushstring(L, errorMsg);
        return 2;
    }

    GS::UniString guidStr = APIGuidToString(createdGuid);
    lua_pushstring(L, guidStr.ToCStr().Get());
    lua_pushfstring(L, "%d overrides applied", appliedOverrides);
    return 2;
}

// createMany(libInd, {posTable, ...}, [params]) — N objects in ONE undo step.
// Returns an array of guid strings (or nil + error; already-placed items stay
// in the model on failure — the message tells how many were placed).
static int CreateManyElements(lua_State* L)
{
    if (!lua_isinteger(L, 1) || !lua_istable(L, 2)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected libInd (integer) and position array arguments");
        return 2;
    }

    Int32 libInd = (Int32)lua_tointeger(L, 1);
    size_t n = lua_rawlen(L, 2);
    if (n == 0 || n > 1000) {
        lua_pushnil(L);
        lua_pushstring(L, "createMany: need 1..1000 positions");
        return 2;
    }
    bool hasParams = lua_istable(L, 3);

    std::vector<API_Guid> placed;
    char errorMsg[256] = {};
    int appliedOverrides = 0;

    GSErrCode err = ACAPI_CallUndoableCommand("Create Elements", [&]() -> GSErrCode {
        API_LibPart libPart;
        BNZeroMemory(&libPart, sizeof(libPart));
        libPart.index = libInd;
        GSErrCode e = ACAPI_LibraryPart_Get(&libPart);
        if (e != NoError) { std::sprintf(errorMsg, "LibraryPart_Get failed: err=%d", (int)e); return e; }
        if (libPart.typeID != APILib_ObjectID) {
            std::sprintf(errorMsg, "unsupported library part type for creation (libType=%d)", (int)libPart.typeID);
            return APIERR_GENERAL;
        }

        for (size_t i = 1; i <= n; ++i) {
            lua_rawgeti(L, 2, (lua_Integer)i);
            if (!lua_istable(L, -1)) {
                lua_pop(L, 1);
                std::sprintf(errorMsg, "createMany: position %d is not a table", (int)i);
                return APIERR_BADPARS;
            }
            double x = 0, y = 0, angle = 0;
            bool hasAngle = false;
            lua_getfield(L, -1, "x");
            if (lua_isnumber(L, -1)) {
                x = lua_tonumber(L, -1);
                lua_pop(L, 1);
                lua_getfield(L, -1, "y");
                if (lua_isnumber(L, -1))
                    y = lua_tonumber(L, -1);
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
            }
            lua_getfield(L, -1, "angle");
            if (lua_isnumber(L, -1)) {
                angle = lua_tonumber(L, -1);
                hasAngle = true;
            }
            lua_pop(L, 1);
            lua_pop(L, 1); // pos table

            API_Element elem;
            BNZeroMemory(&elem, sizeof(elem));
            elem.header.type.typeID = API_ObjectID;
            elem.object.libInd = libInd;

            API_ElementMemo memo;
            BNZeroMemory(&memo, sizeof(memo));
            e = ACAPI_Element_GetDefaults(&elem, &memo);
            if (e != NoError) {
                std::sprintf(errorMsg, "GetDefaults failed at item %d: err=%d", (int)i, (int)e);
                return e;
            }

            elem.object.pos.x = x;
            elem.object.pos.y = y;
            if (hasAngle)
                elem.object.angle = angle;

            // Re-assert libInd: GetDefaults may reset it to the default part.
            elem.object.libInd = libInd;

            e = ACAPI_Element_Create(&elem, &memo);
            ACAPI_DisposeElemMemoHdls(&memo);
            if (e != NoError) {
                std::sprintf(errorMsg, "Element_Create failed at item %d: err=%d", (int)i, (int)e);
                return e;
            }

            // Verify the placed element really is our part (not id=1 default).
            API_Element placedElem;
            BNZeroMemory(&placedElem, sizeof(placedElem));
            placedElem.header.guid = elem.header.guid;
            e = ACAPI_Element_Get(&placedElem);
            if (e != NoError) {
                std::sprintf(errorMsg, "post-create Get failed at item %d: err=%d", (int)i, (int)e);
                return e;
            }
            if (placedElem.header.type.typeID == API_ObjectID &&
                placedElem.object.libInd != libInd) {
                std::sprintf(errorMsg, "item %d placed wrong part: wanted libInd %d, got %d",
                             (int)i, (int)libInd, (int)placedElem.object.libInd);
                return APIERR_GENERAL;
            }

            // Overrides go through Change (Create ignores memo params).
            if (hasParams) {
                API_ElementMemo memo2;
                BNZeroMemory(&memo2, sizeof(memo2));
                e = ACAPI_Element_GetMemo(elem.header.guid, &memo2, APIMemoMask_AddPars);
                if (e != NoError || memo2.params == nullptr) {
                    if (e == NoError)
                        ACAPI_DisposeElemMemoHdls(&memo2);
                    std::sprintf(errorMsg, "createMany: no params memo at item %d", (int)i);
                    return APIERR_GENERAL;
                }
                appliedOverrides += ApplyParamOverrides(L, 3, memo2);
                API_Element mask2;
                ACAPI_ELEMENT_MASK_CLEAR(mask2);
                e = ACAPI_Element_Change(&placedElem, &mask2, &memo2, APIMemoMask_AddPars, true);
                ACAPI_DisposeElemMemoHdls(&memo2);
                if (e != NoError) {
                    std::sprintf(errorMsg, "param change failed at item %d: err=%d", (int)i, (int)e);
                    return e;
                }
            }
            placed.push_back(elem.header.guid);
        }
        if (hasParams && appliedOverrides == 0)
            ACAPI_WriteReport("createMany: warning — none of the override names matched a parameter", false);
        return NoError;
    });

    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "%s (placed %d of %d before failure)", errorMsg, (int)placed.size(), (int)n);
        return 2;
    }

    lua_createtable(L, (int)placed.size(), 0);
    for (size_t i = 0; i < placed.size(); ++i) {
        GS::UniString guidStr = APIGuidToString(placed[i]);
        lua_pushstring(L, guidStr.ToCStr().Get());
        lua_rawseti(L, -2, (lua_Integer)(i + 1));
    }
    lua_pushfstring(L, "%d placed, %d overrides applied", (int)placed.size(), appliedOverrides);
    return 2;
}

// listParams(libInd) — diagnostic: array of {name, typeID, typeMod, value}
// for every parameter of the part. typeMod: 0=simple, 2=array.
static int ListParams(lua_State* L)
{
    if (!lua_isinteger(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected libInd (integer)");
        return 2;
    }

    Int32 libInd = (Int32)lua_tointeger(L, 1);

    API_ParamOwnerType paramOwner;
    BNZeroMemory(&paramOwner, sizeof(paramOwner));
    paramOwner.libInd = libInd;
    GSErrCode err = ACAPI_LibraryPart_OpenParameters(&paramOwner);
    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "OpenParameters failed: err=%d", (int)err);
        return 2;
    }

    API_GetParamsType getParams;
    BNZeroMemory(&getParams, sizeof(getParams));
    err = ACAPI_LibraryPart_GetActParameters(&getParams);
    if (err != NoError || getParams.params == nullptr) {
        ACAPI_LibraryPart_CloseParameters();
        lua_pushnil(L);
        lua_pushstring(L, "no parameters");
        return 2;
    }

    UInt32 nParams = (UInt32)(BMGetHandleSize((GSHandle)getParams.params) / sizeof(API_AddParType));
    lua_createtable(L, (int)nParams, 0);
    for (UInt32 i = 0; i < nParams; ++i) {
        API_AddParType& par = (*getParams.params)[i];
        lua_createtable(L, 0, 4);
        lua_pushstring(L, par.name);
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, par.typeID);
        lua_setfield(L, -2, "typeID");
        lua_pushinteger(L, par.typeMod);
        lua_setfield(L, -2, "typeMod");
        switch (par.typeID) {
            case APIParT_Integer: case APIParT_Length:
            case APIParT_Angle: case APIParT_RealNum: case APIParT_Intens:
            case APIParT_LightSw: case APIParT_Boolean:
                lua_pushnumber(L, par.value.real);
                lua_setfield(L, -2, "value");
                break;
            case APIParT_CString: case APIParT_Title:
                lua_pushstring(L, GS::UniString(par.value.uStr).ToCStr().Get());
                lua_setfield(L, -2, "value");
                break;
            default:
                break;
        }
        lua_rawseti(L, -2, (lua_Integer)(i + 1));
    }
    ACAPI_LibraryPart_CloseParameters();
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

static bool SetMemoGDLParam(API_ElementMemo& memo, const char* paramName, bool isString, const char* valueStr, double value)
{
    if (!memo.params)
        return false;
    UInt32 n = (UInt32)(BMGetHandleSize((GSHandle)memo.params) / sizeof(API_AddParType));
    for (UInt32 i = 0; i < n; ++i) {
        API_AddParType& par = (*memo.params)[i];
        if (par.typeMod != API_ParSimple)
            continue;
        if (GS::UniString(par.name) != paramName)
            continue;
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
        return true;
    }
    return false;
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

    // Read mirrored / openingAngle params before any element setup
    lua_getfield(L, 2, "mirrored");
    bool mirrored = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "openingAngle");
    double openingAngle = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 45.0;
    lua_pop(L, 1);

    // Wall sides: refSide/oSide are plain mirror flags ("outside" = true).
    // They map 1:1 to the stored booleans; no wall-flip compensation.
    auto readSideFlag = [&](const char* key) -> bool {
        lua_getfield(L, 2, key);
        bool outside = false;
        if (lua_isstring(L, -1)) {
            const char* s = lua_tostring(L, -1);
            outside = (GS::UniString(s).Compare("outside") == 0);
        } else if (lua_isinteger(L, -1)) {
            outside = (lua_tointeger(L, -1) != 0);
        }
        lua_pop(L, 1);
        return outside;
    };
    bool refSideOutside = readSideFlag("refSide");
    bool oSideOutside = readSideFlag("oSide");

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
    elem.window.openingBase.refSide = refSideOutside;
    elem.window.openingBase.oSide = oSideOutside;

    // Set ac_OpeningSide (A=inside, B=outside) in the memo before Create.
    // A post-create Change of this GDL param does not flip the window; it must be
    // applied to the parameter memo passed to ACAPI_Element_Create.
    SetMemoGDLParam(memo, "ac_OpeningSide", true, oSideOutside ? "B" : "A", 0.0);
    SetMemoGDLParam(memo, "gs_open_2D", false, nullptr, openingAngle);

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

    // Post-create: re-apply mirror flag via Change (redundant safety, keeps param state in sync)
    ACAPI_CallUndoableCommand("Post-create Window", [&]() -> GSErrCode {
        API_Element elem2;
        BNZeroMemory(&elem2, sizeof(elem2));
        elem2.header.guid = createdGuid;
        ACAPI_Element_Get(&elem2);
        API_Element mask2;
        ACAPI_ELEMENT_MASK_CLEAR(mask2);
        elem2.window.openingBase.reflected = mirrored;
        elem2.window.openingBase.refSide = refSideOutside;
        elem2.window.openingBase.oSide = oSideOutside;
        ACAPI_ELEMENT_MASK_SET(mask2, API_WindowType, openingBase.reflected);
        ACAPI_ELEMENT_MASK_SET(mask2, API_WindowType, openingBase.refSide);
        ACAPI_ELEMENT_MASK_SET(mask2, API_WindowType, openingBase.oSide);
        return ACAPI_Element_Change(&elem2, &mask2, nullptr, 0, true);
    });

    GS::UniString guidStr = APIGuidToString(createdGuid);
    lua_pushstring(L, guidStr.ToCStr().Get());
    return 1;
}

static int GetWindow(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (!guidStr) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a window GUID");
        return 2;
    }

    API_Guid guid = APIGuidFromString(guidStr);
    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get(&elem);
    if (err != NoError || elem.header.type.typeID != API_WindowID) {
        lua_pushnil(L);
        lua_pushstring(L, "window not found");
        return 2;
    }

    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));
    double openingAngle = 45.0;
    if (ACAPI_Element_GetMemo(guid, &memo, APIMemoMask_AddPars) == NoError && memo.params != nullptr) {
        UInt32 n = (UInt32)(BMGetHandleSize((GSHandle)memo.params) / sizeof(API_AddParType));
        for (UInt32 i = 0; i < n; ++i) {
            API_AddParType& par = (*memo.params)[i];
            if (GS::UniString(par.name) == "gs_open_2D" && par.typeMod == API_ParSimple) {
                openingAngle = par.value.real * 180.0 / 3.14159265358979323846;
                break;
            }
        }
        ACAPI_DisposeElemMemoHdls(&memo);
    }

    lua_createtable(L, 0, 10);
    PushGuid(L, elem.header.guid);
    lua_setfield(L, -2, "guid");
    PushGuid(L, elem.window.owner);
    lua_setfield(L, -2, "wallGuid");
    lua_pushnumber(L, elem.window.objLoc);
    lua_setfield(L, -2, "objLoc");
    lua_pushnumber(L, elem.window.openingBase.height);
    lua_setfield(L, -2, "height");
    lua_pushnumber(L, elem.window.openingBase.width);
    lua_setfield(L, -2, "width");
    lua_pushnumber(L, elem.window.lower);
    lua_setfield(L, -2, "sillHeight");

    lua_pushstring(L, elem.window.openingBase.refSide ? "outside" : "inside");
    lua_setfield(L, -2, "refSide");

    lua_pushstring(L, elem.window.openingBase.oSide ? "outside" : "inside");
    lua_setfield(L, -2, "oSide");

    lua_pushboolean(L, elem.window.openingBase.reflected);
    lua_setfield(L, -2, "mirrored");
    lua_pushnumber(L, openingAngle);
    lua_setfield(L, -2, "openingAngle");

    return 1;
}

static int SetWindow(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (!guidStr || !lua_istable(L, 2)) {
        lua_pushnil(L);
        lua_pushstring(L, "expected a window GUID and params table");
        return 2;
    }

    API_Guid guid = APIGuidFromString(guidStr);
    API_Element elem;
    BNZeroMemory(&elem, sizeof(elem));
    elem.header.guid = guid;
    GSErrCode err = ACAPI_Element_Get(&elem);
    if (err != NoError || elem.header.type.typeID != API_WindowID) {
        lua_pushnil(L);
        lua_pushstring(L, "window not found");
        return 2;
    }

    // Snapshot current state: only actually-changed fields are written, so an
    // update with unchanged values is a true no-op (never mirrors the window).
    const double oldObjLoc = elem.window.objLoc;
    const double oldHeight = elem.window.openingBase.height;
    const double oldWidth = elem.window.openingBase.width;
    const double oldLower = elem.window.lower;
    const bool oldReflected = elem.window.openingBase.reflected;
    const bool oldRefSide = elem.window.openingBase.refSide;
    const bool oldOSide = elem.window.openingBase.oSide;

    lua_getfield(L, 2, "objLoc");
    if (lua_isnumber(L, -1)) elem.window.objLoc = lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "height");
    if (lua_isnumber(L, -1)) elem.window.openingBase.height = lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "width");
    if (lua_isnumber(L, -1)) elem.window.openingBase.width = lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "sillHeight");
    if (lua_isnumber(L, -1)) elem.window.lower = lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "mirrored");
    if (lua_isboolean(L, -1)) elem.window.openingBase.reflected = lua_toboolean(L, -1);
    lua_pop(L, 1);

    auto readSideFlag = [&](const char* key, bool& targetField) {
        lua_getfield(L, 2, key);
        if (lua_isstring(L, -1)) {
            const char* s = lua_tostring(L, -1);
            targetField = (GS::UniString(s).Compare("outside") == 0);
        } else if (lua_isboolean(L, -1)) {
            targetField = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
    };
    readSideFlag("refSide", elem.window.openingBase.refSide);
    readSideFlag("oSide", elem.window.openingBase.oSide);

    bool angleKeyPresent = false;
    double openingAngle = -1.0;
    lua_getfield(L, 2, "openingAngle");
    if (lua_isnumber(L, -1)) {
        angleKeyPresent = true;
        openingAngle = lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    API_Element mask;
    ACAPI_ELEMENT_MASK_CLEAR(mask);
    if (elem.window.objLoc != oldObjLoc)
        ACAPI_ELEMENT_MASK_SET(mask, API_WindowType, objLoc);
    if (elem.window.openingBase.height != oldHeight)
        ACAPI_ELEMENT_MASK_SET(mask, API_WindowType, openingBase.height);
    if (elem.window.openingBase.width != oldWidth)
        ACAPI_ELEMENT_MASK_SET(mask, API_WindowType, openingBase.width);
    if (elem.window.lower != oldLower)
        ACAPI_ELEMENT_MASK_SET(mask, API_WindowType, lower);
    if (elem.window.openingBase.reflected != oldReflected)
        ACAPI_ELEMENT_MASK_SET(mask, API_WindowType, openingBase.reflected);
    if (elem.window.openingBase.refSide != oldRefSide)
        ACAPI_ELEMENT_MASK_SET(mask, API_WindowType, openingBase.refSide);
    const bool oSideChanged = (elem.window.openingBase.oSide != oldOSide);
    if (oSideChanged)
        ACAPI_ELEMENT_MASK_SET(mask, API_WindowType, openingBase.oSide);

    // GDL params are rewritten only when their logical value actually changed,
    // otherwise a gratuitous ac_OpeningSide rewrite would mirror the window.
    API_ElementMemo memo;
    BNZeroMemory(&memo, sizeof(memo));
    bool memoModified = false;
    if (ACAPI_Element_GetMemo(guid, &memo, APIMemoMask_AddPars) == NoError && memo.params != nullptr) {
        if (oSideChanged) {
            if (SetMemoGDLParam(memo, "ac_OpeningSide", true, elem.window.openingBase.oSide ? "B" : "A", 0.0))
                memoModified = true;
        }
        if (angleKeyPresent) {
            double currentAngle = 0.0;
            bool haveAngle = false;
            UInt32 n = (UInt32)(BMGetHandleSize((GSHandle)memo.params) / sizeof(API_AddParType));
            for (UInt32 i = 0; i < n; ++i) {
                API_AddParType& par = (*memo.params)[i];
                if (GS::UniString(par.name) == "gs_open_2D" && par.typeMod == API_ParSimple) {
                    currentAngle = par.value.real * 180.0 / 3.14159265358979323846;
                    haveAngle = true;
                    break;
                }
            }
            if (!haveAngle || fabs(currentAngle - openingAngle) > 1e-9) {
                if (SetMemoGDLParam(memo, "gs_open_2D", false, nullptr, openingAngle))
                    memoModified = true;
            }
        }
    }

    const bool elemChanged =
        elem.window.objLoc != oldObjLoc ||
        elem.window.openingBase.height != oldHeight ||
        elem.window.openingBase.width != oldWidth ||
        elem.window.lower != oldLower ||
        elem.window.openingBase.reflected != oldReflected ||
        elem.window.openingBase.refSide != oldRefSide ||
        oSideChanged;

    if (!elemChanged && !memoModified) {
        if (memo.params != nullptr)
            ACAPI_DisposeElemMemoHdls(&memo);
        lua_pushboolean(L, true);
        return 1;
    }

    err = ACAPI_CallUndoableCommand("Modify Window", [&]() -> GSErrCode {
        return ACAPI_Element_Change(&elem, &mask, memoModified ? &memo : nullptr, memoModified ? APIMemoMask_AddPars : 0, true);
    });

    if (memo.params != nullptr) {
        ACAPI_DisposeElemMemoHdls(&memo);
    }

    if (err != NoError) {
        lua_pushnil(L);
        lua_pushfstring(L, "change window failed: err=%d", (int)err);
        return 2;
    }

    lua_pushboolean(L, true);
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

    lua_getfield(L, 2, "mirrored");
    bool mirrored = lua_toboolean(L, -1);
    lua_pop(L, 1);

    // Wall sides: refSide/oSide are plain mirror flags ("outside" = true).
    // They map 1:1 to the stored booleans; no wall-flip compensation.
    auto readSideFlag = [&](const char* key) -> bool {
        lua_getfield(L, 2, key);
        bool outside = false;
        if (lua_isstring(L, -1)) {
            const char* s = lua_tostring(L, -1);
            outside = (GS::UniString(s).Compare("outside") == 0);
        } else if (lua_isinteger(L, -1)) {
            outside = (lua_tointeger(L, -1) != 0);
        }
        lua_pop(L, 1);
        return outside;
    };
    bool refSideOutside = readSideFlag("refSide");
    bool oSideOutside = readSideFlag("oSide");

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
    elem.door.openingBase.reflected = mirrored;
    elem.door.openingBase.refSide = refSideOutside;
    elem.door.openingBase.oSide = oSideOutside;

    // Set ac_OpeningSide (A=inside, B=outside) in the memo before Create.
    SetMemoGDLParam(memo, "ac_OpeningSide", true, oSideOutside ? "B" : "A", 0.0);

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

// ---------------------------------------------------------------------------
// Windows Registry persistence, strictly scoped to
// HKCU\Software\Samu\ArchiLua\<section>\ (section/key = [A-Za-z0-9_]+ only).
// Values are stored as REG_SZ; numbers/booleans round-trip as strings.
// NOTE: CommonLibs WinReg helpers were NOT used here: GetRegString() silently
// drops values > 255 chars (fixed buffer, return code ignored) and
// GetOrCreateRegPath() uses the HKEY unchecked. The helpers below size the
// read buffer in two steps and check every LSTATUS.
// ---------------------------------------------------------------------------

static bool RegNameValid(const char* s)
{
    if (s == nullptr || *s == '\0')
        return false;
    size_t len = 0;
    for (const char* p = s; *p != '\0'; ++p) {
        char c = *p;
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_';
        if (!ok)
            return false;
        if (++len > 64)
            return false;
    }
    return true;
}

static std::wstring RegToWide(const char* utf8)
{
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (wlen <= 0)
        return std::wstring();
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], wlen);
    w.resize(wlen > 0 ? (size_t)(wlen - 1) : 0);
    return w;
}

static std::string RegToUtf8(const wchar_t* w, size_t wlen)
{
    int mlen = WideCharToMultiByte(CP_UTF8, 0, w, (int)wlen, nullptr, 0, nullptr, nullptr);
    if (mlen <= 0)
        return std::string();
    std::string s(mlen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, (int)wlen, &s[0], mlen, nullptr, nullptr);
    return s;
}

static int RegRead(lua_State* L)
{
    const char* section = lua_tostring(L, 1);
    const char* key = lua_tostring(L, 2);
    if (!RegNameValid(section) || !RegNameValid(key)) {
        lua_pushnil(L);
        lua_pushstring(L, "regRead: section and key must be [A-Za-z0-9_]{1,64}");
        return 2;
    }

    std::wstring subkey = L"Software\\Samu\\ArchiLua\\";
    subkey += RegToWide(section);
    std::wstring wkey = RegToWide(key);

    DWORD size = 0;
    LSTATUS st = RegGetValueW(HKEY_CURRENT_USER, subkey.c_str(), wkey.c_str(),
                              RRF_RT_REG_SZ, nullptr, nullptr, &size);
    if (st != ERROR_SUCCESS || size == 0 || size > 65536) {
        if (lua_gettop(L) >= 3)
            lua_pushvalue(L, 3); // default
        else
            lua_pushnil(L);
        return 1;
    }

    std::wstring wval(size / sizeof(wchar_t), L'\0');
    st = RegGetValueW(HKEY_CURRENT_USER, subkey.c_str(), wkey.c_str(),
                      RRF_RT_REG_SZ, nullptr, &wval[0], &size);
    if (st != ERROR_SUCCESS) {
        if (lua_gettop(L) >= 3)
            lua_pushvalue(L, 3);
        else
            lua_pushnil(L);
        return 1;
    }

    size_t wlen = size / sizeof(wchar_t);
    while (wlen > 0 && wval[wlen - 1] == L'\0')
        --wlen;
    std::string out = RegToUtf8(wval.c_str(), wlen);
    lua_pushstring(L, out.c_str());
    return 1;
}

static int RegWrite(lua_State* L)
{
    const char* section = lua_tostring(L, 1);
    const char* key = lua_tostring(L, 2);
    if (!RegNameValid(section) || !RegNameValid(key)) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "regWrite: section and key must be [A-Za-z0-9_]{1,64}");
        return 2;
    }

    std::string value;
    if (lua_type(L, 3) == LUA_TSTRING)
        value = lua_tostring(L, 3);
    else if (lua_isnumber(L, 3))
        value = lua_tostring(L, 3);
    else if (lua_isboolean(L, 3))
        value = lua_toboolean(L, 3) ? "true" : "false";
    else {
        lua_pushboolean(L, false);
        lua_pushstring(L, "regWrite: value must be string, number or boolean");
        return 2;
    }
    if (value.size() > 4000) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "regWrite: value too long (max 4000 chars)");
        return 2;
    }

    std::wstring subkey = L"Software\\Samu\\ArchiLua\\";
    subkey += RegToWide(section);
    std::wstring wkey = RegToWide(key);
    std::wstring wval = RegToWide(value.c_str());

    HKEY hKey = nullptr;
    LSTATUS st = RegCreateKeyExW(HKEY_CURRENT_USER, subkey.c_str(), 0, nullptr,
                                 REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr);
    if (st != ERROR_SUCCESS || hKey == nullptr) {
        lua_pushboolean(L, false);
        lua_pushfstring(L, "regWrite: cannot open key (%d)", (int)st);
        return 2;
    }

    st = RegSetValueExW(hKey, wkey.c_str(), 0, REG_SZ,
                        reinterpret_cast<const BYTE*>(wval.c_str()),
                        (DWORD)((wval.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    if (st != ERROR_SUCCESS) {
        lua_pushboolean(L, false);
        lua_pushfstring(L, "regWrite failed (%d)", (int)st);
        return 2;
    }
    lua_pushboolean(L, true);
    return 1;
}

// ---------------------------------------------------------------------------
// Minimalist element observer (Phase 3.7 PoC).
// Session map: watched-guid -> {func_url, kwargs registry ref}.
// Persisted as JSON text in project ModulData ("watches") so watches survive
// project reload. Edit bursts are coalesced: Lua runs once for the last Edit
// in a row (flushed on Change, leftovers on EndEvents) plus once on Change.
// A depth guard skips notifications caused by our own callback's DB writes.
// ---------------------------------------------------------------------------

namespace {
struct WatchEntry {
    std::string funcUrl;
    int         kwargsRef = LUA_NOREF;
};
}

static std::map<std::string, WatchEntry>& WatchMap()
{
    static std::map<std::string, WatchEntry> m;
    return m;
}

static std::map<std::string, bool>& PendingEdits()
{
    static std::map<std::string, bool> m;
    return m;
}

static lua_State*& ObserverState()
{
    static lua_State* L = nullptr;
    return L;
}

static LuaDebugger*& ObserverDebugger()
{
    static LuaDebugger* d = nullptr;
    return d;
}

static int& DispatchDepth()
{
    static int d = 0;
    return d;
}

static bool& ObserverLogEnabled()
{
    static bool on = false;
    return on;
}

inline void SetObserverContext(lua_State* L, LuaDebugger* dbg)
{
    ObserverState() = L;
    ObserverDebugger() = dbg;
}

// Lua table -> Json (mirror of PushJsonToLua). Non-table scalars convert
// directly; functions/userdata become null. Depth-limited (cycle guard).
static Json LuaTableToJson(lua_State* L, int idx, int depth = 0)
{
    Json v;
    if (depth > 8)
        return v;
    idx = lua_absindex(L, idx);
    switch (lua_type(L, idx)) {
        case LUA_TNIL:
            v.type = Json::Null;
            break;
        case LUA_TBOOLEAN:
            v.type = Json::Bool;
            v.b = lua_toboolean(L, idx) != 0;
            break;
        case LUA_TNUMBER:
            v.type = Json::Num;
            v.n = lua_tonumber(L, idx);
            break;
        case LUA_TSTRING:
            v.type = Json::Str;
            v.s = lua_tostring(L, idx);
            break;
        case LUA_TTABLE: {
            size_t count = 0;
            bool isArr = true;
            lua_pushnil(L);
            while (lua_next(L, idx) != 0) {
                if (lua_type(L, -2) != LUA_TNUMBER || !lua_isinteger(L, -2) || lua_tointeger(L, -2) < 1)
                    isArr = false;
                ++count;
                lua_pop(L, 1);
            }
            if (isArr && count > 0) {
                v.type = Json::Arr;
                for (size_t i = 1; i <= count; ++i) {
                    lua_rawgeti(L, idx, (lua_Integer)i);
                    v.a.push_back(LuaTableToJson(L, -1, depth + 1));
                    lua_pop(L, 1);
                }
            } else {
                v.type = Json::Obj;
                lua_pushnil(L);
                while (lua_next(L, idx) != 0) {
                    std::string key;
                    if (lua_type(L, -2) == LUA_TSTRING)
                        key = lua_tostring(L, -2);
                    else if (lua_isnumber(L, -2))
                        key = lua_tostring(L, -2);
                    else {
                        lua_pop(L, 1);
                        continue;
                    }
                    v.o.push_back({key, LuaTableToJson(L, -1, depth + 1)});
                    lua_pop(L, 1);
                }
            }
            break;
        }
        default:
            v.type = Json::Null;
            break;
    }
    return v;
}

// Json -> Lua (local mirror of LuaWebDialog::PushJsonToLua, avoids coupling).
static void PushJsonVal(lua_State* L, const Json& val)
{
    switch (val.type) {
        case Json::Null:
            lua_pushnil(L);
            break;
        case Json::Bool:
            lua_pushboolean(L, val.b);
            break;
        case Json::Num:
            lua_pushnumber(L, val.n);
            break;
        case Json::Str:
            lua_pushstring(L, val.s.c_str());
            break;
        case Json::Arr:
            lua_newtable(L);
            for (size_t i = 0; i < val.a.size(); ++i) {
                PushJsonVal(L, val.a[i]);
                lua_rawseti(L, -2, (int)(i + 1));
            }
            break;
        case Json::Obj:
            lua_newtable(L);
            for (size_t i = 0; i < val.o.size(); ++i) {
                lua_pushstring(L, val.o[i].first.c_str());
                PushJsonVal(L, val.o[i].second);
                lua_rawset(L, -3);
            }
            break;
    }
}

static void PersistWatches()
{
    lua_State* L = ObserverState();
    if (L == nullptr)
        return;
    if (WatchMap().empty()) {
        ACAPI_ModulData_Delete(GS::UniString("watches"));
        return;
    }
    Json root;
    root.type = Json::Obj;
    for (const auto& [guid, entry] : WatchMap()) {
        Json item;
        item.type = Json::Obj;
        Json url;
        url.type = Json::Str;
        url.s = entry.funcUrl;
        item.o.push_back({"func_url", url});
        Json kw;
        kw.type = Json::Obj;
        if (entry.kwargsRef != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, entry.kwargsRef);
            kw = LuaTableToJson(L, -1);
            lua_pop(L, 1);
            if (kw.type != Json::Obj)
                kw.type = Json::Obj;
        }
        item.o.push_back({"kwargs", kw});
        root.o.push_back({guid, item});
    }
    std::string text = root.Dump();
    GSHandle h = BMAllocateHandle((GSSize)text.size() + 1, 0, 0);
    if (h == nullptr)
        return;
    memcpy(*h, text.c_str(), text.size() + 1);
    API_ModulData md = {};
    md.dataVersion = 1;
    md.platformSign = GS::Win_Platform_Sign;
    md.dataHdl = h;
    ACAPI_ModulData_Store(&md, GS::UniString("watches"));
    BMKillHandle(&h);
}

static void RestoreWatches()
{
    lua_State* L = ObserverState();
    if (L == nullptr)
        return;
    API_ModulData md = {};
    if (ACAPI_ModulData_Get(&md, GS::UniString("watches")) != NoError || md.dataHdl == nullptr)
        return;
    GSSize sz = BMGetHandleSize(md.dataHdl);
    std::string text(*md.dataHdl, (size_t)sz);
    BMKillHandle(&md.dataHdl);
    Json root = Json::Parse(text);
    if (root.type != Json::Obj)
        return;
    bool pruned = false;
    for (size_t i = 0; i < root.o.size(); ++i) {
        const std::string& guidStr = root.o[i].first;
        const Json& item = root.o[i].second;
        if (item.type != Json::Obj)
            continue;
        std::string funcUrl;
        Json kwargs;
        kwargs.type = Json::Obj;
        for (size_t k = 0; k < item.o.size(); ++k) {
            if (item.o[k].first == "func_url" && item.o[k].second.type == Json::Str)
                funcUrl = item.o[k].second.s;
            else if (item.o[k].first == "kwargs" && item.o[k].second.type == Json::Obj)
                kwargs = item.o[k].second;
        }
        if (funcUrl.empty()) {
            pruned = true;
            continue;
        }
        API_Guid guid = APIGuidFromString(guidStr.c_str());
        API_Element elem;
        BNZeroMemory(&elem, sizeof(elem));
        elem.header.guid = guid;
        if (ACAPI_Element_Get(&elem) != NoError) {
            pruned = true; // element gone (deleted since save) — drop the watch
            continue;
        }
        if (ACAPI_Element_AttachObserver(guid, 0) != NoError) {
            pruned = true;
            continue;
        }
        PushJsonVal(L, kwargs);
        WatchEntry entry;
        entry.funcUrl = funcUrl;
        entry.kwargsRef = luaL_ref(L, LUA_REGISTRYINDEX);
        WatchMap()[guidStr] = entry;
    }
    if (pruned)
        PersistWatches();
}

static void DispatchWatch(const std::string& guidStr, const char* kind)
{
    auto it = WatchMap().find(guidStr);
    if (it == WatchMap().end())
        return;
    lua_State* L = ObserverState();
    if (L == nullptr)
        return;
    std::string func = it->second.funcUrl;
    size_t bs = func.find('\\');
    if (bs != std::string::npos)
        func = func.substr(bs + 1); // "script.lua\Func" -> Func (script part informational for now)
    lua_getglobal(L, func.c_str());
    if (!lua_isfunction(L, -1)) {
        ACAPI_WriteReport(("watch: function not found: " + func).c_str(), true);
        lua_pop(L, 1);
        return;
    }
    lua_pushstring(L, guidStr.c_str());
    if (it->second.kwargsRef != LUA_NOREF)
        lua_rawgeti(L, LUA_REGISTRYINDEX, it->second.kwargsRef);
    else
        lua_newtable(L);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    lua_pushstring(L, kind);

    LuaDebugger* dbg = ObserverDebugger();
    bool hadHook = false;
    if (dbg != nullptr && dbg->HasClient()) {
        LuaDebugger::ActivateForCallback(dbg);
        lua_sethook(L, LuaDebugger::DebugHook, LUA_MASKLINE, 0);
        hadHook = true;
    }
    ++DispatchDepth();
    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err)
            ACAPI_WriteReport(err, true);
        lua_pop(L, 1);
    }
    --DispatchDepth();
    if (hadHook) {
        lua_sethook(L, nullptr, 0, 0);
        LuaDebugger::DeactivateForCallback();
    }
}

static void FlushPendingEdits()
{
    std::vector<std::string> guids;
    for (const auto& [guid, pending] : PendingEdits()) {
        if (pending)
            guids.push_back(guid);
    }
    PendingEdits().clear();
    for (const std::string& guid : guids)
        DispatchWatch(guid, "edit");
}

static GSErrCode __ACENV_CALL ObserverHandler(const API_NotifyElementType* et)
{
    if (et == nullptr)
        return NoError;
    if (ObserverLogEnabled()) {
        GS::UniString guidU = APIGuidToString(et->elemHead.guid);
        std::string msg = "observer: notif=" + std::to_string((int)et->notifID) +
                          " guid=" + guidU.ToCStr().Get();
        ACAPI_WriteReport(msg.c_str(), false);
    }
    if (DispatchDepth() > 0)
        return NoError; // nested notification from our own callback's DB writes
    if (et->notifID == APINotifyElement_EndEvents) {
        FlushPendingEdits(); // burst ended (incl. drag-cancel with no Change)
        return NoError;
    }
    if (et->notifID != APINotifyElement_Change && et->notifID != APINotifyElement_Edit)
        return NoError; // ignore property/classification/undo/redo/etc.
    GS::UniString guidU = APIGuidToString(et->elemHead.guid);
    std::string guidStr(guidU.ToCStr().Get());
    if (WatchMap().find(guidStr) == WatchMap().end())
        return NoError;
    if (et->notifID == APINotifyElement_Edit) {
        PendingEdits()[guidStr] = true; // coalesce: Lua runs once for the last Edit
        return NoError;
    }
    auto pit = PendingEdits().find(guidStr);
    if (pit != PendingEdits().end()) {
        PendingEdits().erase(pit);
        DispatchWatch(guidStr, "edit");
    }
    DispatchWatch(guidStr, "change");
    return NoError;
}

static int Watch(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    const char* funcUrl = lua_tostring(L, 2);
    if (guidStr == nullptr || funcUrl == nullptr || funcUrl[0] == '\0') {
        lua_pushboolean(L, false);
        lua_pushstring(L, "watch: expected guid, func_url[, kwargs table]");
        return 2;
    }
    if (!lua_istable(L, 3) && !lua_isnoneornil(L, 3)) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "watch: kwargs must be a table or nil");
        return 2;
    }
    API_Guid guid = APIGuidFromString(guidStr);
    ACAPI_Element_DetachObserver(guid); // start clean: re-watch must not fail as "already attached"
    GSErrCode err = ACAPI_Element_AttachObserver(guid, 0);
    if (err != NoError) {
        lua_pushboolean(L, false);
        lua_pushfstring(L, "AttachObserver failed: err=%d", (int)err);
        return 2;
    }
    auto& map = WatchMap();
    auto it = map.find(guidStr);
    if (it != map.end() && it->second.kwargsRef != LUA_NOREF)
        luaL_unref(L, LUA_REGISTRYINDEX, it->second.kwargsRef);
    WatchEntry entry;
    entry.funcUrl = funcUrl;
    if (lua_istable(L, 3))
        lua_pushvalue(L, 3);
    else
        lua_newtable(L);
    entry.kwargsRef = luaL_ref(L, LUA_REGISTRYINDEX);
    map[guidStr] = entry;
    PendingEdits().erase(guidStr);
    PersistWatches();
    lua_pushboolean(L, true);
    return 1;
}

static int Unwatch(lua_State* L)
{
    const char* guidStr = lua_tostring(L, 1);
    if (guidStr == nullptr) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "unwatch: expected guid");
        return 2;
    }
    API_Guid guid = APIGuidFromString(guidStr);
    ACAPI_Element_DetachObserver(guid); // ignore errors (e.g. element already gone)
    auto& map = WatchMap();
    auto it = map.find(guidStr);
    if (it != map.end()) {
        if (it->second.kwargsRef != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, it->second.kwargsRef);
        map.erase(it);
        PersistWatches();
    }
    PendingEdits().erase(guidStr);
    lua_pushboolean(L, true);
    return 1;
}

static int ObserverLog(lua_State* L)
{
    ObserverLogEnabled() = lua_toboolean(L, 1) != 0;
    lua_pushboolean(L, true);
    return 1;
}

inline void InstallObserver()
{
    ACAPI_Element_InstallElementObserver(ObserverHandler);
    RestoreWatches();
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

    lua_pushcfunction(L, DeleteElement);
    lua_setfield(L, -2, "delete");

    lua_pushcfunction(L, SetParams);
    lua_setfield(L, -2, "setParams");

    lua_pushcfunction(L, FindObject);
    lua_setfield(L, -2, "findObject");

    lua_pushcfunction(L, CreateElement);
    lua_setfield(L, -2, "create");

    lua_pushcfunction(L, CreateManyElements);
    lua_setfield(L, -2, "createMany");

    lua_pushcfunction(L, ListParams);
    lua_setfield(L, -2, "listParams");

    lua_pushcfunction(L, AddWall);
    lua_setfield(L, -2, "addWall");

    lua_pushcfunction(L, AddWindow);
    lua_setfield(L, -2, "addWindow");

    lua_pushcfunction(L, GetWindow);
    lua_setfield(L, -2, "getWindow");

    lua_pushcfunction(L, SetWindow);
    lua_setfield(L, -2, "setWindow");

    lua_pushcfunction(L, AddDoor);
    lua_setfield(L, -2, "addDoor");

    lua_pushcfunction(L, AddSlab);
    lua_setfield(L, -2, "addSlab");

    lua_pushcfunction(L, AddRoof);
    lua_setfield(L, -2, "addRoof");

    lua_pushcfunction(L, SetGDLParam);
    lua_setfield(L, -2, "setGDLParam");

    lua_pushcfunction(L, Watch);
    lua_setfield(L, -2, "watch");

    lua_pushcfunction(L, Unwatch);
    lua_setfield(L, -2, "unwatch");

    lua_pushcfunction(L, ObserverLog);
    lua_setfield(L, -2, "observerLog");

    lua_pushcfunction(L, RegRead);
    lua_setfield(L, -2, "regRead");

    lua_pushcfunction(L, RegWrite);
    lua_setfield(L, -2, "regWrite");

    lua_setglobal(L, "acapi");
}

} // namespace APIModule
} // namespace ArchiLua