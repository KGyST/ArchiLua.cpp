// ArchiLua - ArchiCAD <-> Lua 5.4 bridge
// API Development Kit 27; Win

// ---------------------------------- Includes ---------------------------------

#include "APIEnvir.h"
#include "ACAPinc.h"
#include "AC27/APICommon.h"

#include "ArchiLua_Resource.h"
#include "ArchiLua.hpp"
#include "Bridge/LuaBridge.hpp"
#include "Console/LuaConsole.hpp"
#include "Gui/LuaScriptDialog.hpp"

#include "Logger/Logger.hpp"

using namespace ArchiLua;

// ---------------------------------- Variables --------------------------------

static Bridge luaBridge;
Logger logger(COMPANY_NAME, APP_NAME);

Bridge& ArchiLua::GetBridge()
{
    return luaBridge;
}

// =============================================================================
//
// Menu command handler
//
// =============================================================================

static GSErrCode __ACENV_CALL MenuCommandHandler(const API_MenuParams* params)
{
    switch (params->menuItemRef.itemIndex) {
    case 1:
        {
            auto* dlg = new LuaScriptDialog();
            GetBridge().SetDialog(dlg);
            dlg->Show();
        }
        break;
    }
    return NoError;
}

// =============================================================================
//
// Required functions
//
// =============================================================================

//------------------------------------------------------
// Dependency definitions
//------------------------------------------------------
API_AddonType __ACENV_CALL CheckEnvironment(API_EnvirParams* envir)
{
    if (envir->serverInfo.serverApplication != APIAppl_ArchiCADID)
        return APIAddon_DontRegister;

    RSGetIndString(&envir->addOnInfo.name, IDS_ADDON_NAME, 1, ACAPI_GetOwnResModule());
    RSGetIndString(&envir->addOnInfo.description, IDS_ADDON_NAME, 2, ACAPI_GetOwnResModule());
#ifdef _DEBUG
    envir->addOnInfo.name.Insert(0, L'_');
#endif

    return APIAddon_Normal;
}

//------------------------------------------------------
// Interface definitions
//------------------------------------------------------
GSErrCode __ACENV_CALL RegisterInterface(void)
{
    return ACAPI_MenuItem_RegisterMenu(IDS_APP_NAME, 0, MenuCode_UserDef, MenuFlag_Default);
}

//------------------------------------------------------
// Called when the Add-On has been loaded into memory
//------------------------------------------------------
GSErrCode __ACENV_CALL Initialize(void)
{
    GSErrCode err = NoError;

    err = ACAPI_MenuItem_InstallMenuHandler(IDS_APP_NAME, MenuCommandHandler);
    if (err != NoError)
        DBPrintf("ArchiLua::Initialize() ACAPI_Install_MenuHandler failed\n");

    // Start Lua state + DAP server so VSCodium can attach before dialog opens
    GetBridge().Init();

    // Register modeless window so ArchiCAD doesn't unload the add-on while the palette is open
    ACAPI_RegisterModelessWindow(LuaScriptDialog::PaletteRefId(),
                                 LuaScriptDialog::PaletteAPIControlCallBack,
                                 API_PalEnabled_FloorPlan + API_PalEnabled_Section + API_PalEnabled_Elevation +
                                 API_PalEnabled_InteriorElevation + API_PalEnabled_3D +
                                 API_PalEnabled_Detail + API_PalEnabled_Worksheet + API_PalEnabled_Layout,
                                 GSGuid2APIGuid(LuaScriptDialog::PaletteGuid()));

    return err;
}

//------------------------------------------------------
// FreeData
//------------------------------------------------------
GSErrCode __ACENV_CALL FreeData(void)
{
    ACAPI_UnregisterModelessWindow(LuaScriptDialog::PaletteRefId());
    GetBridge().CloseDialog();
    return NoError;
}

// =============================================================================
//
// Undo state
//
// =============================================================================

namespace {

struct UndoState {
    bool                     active = false;
    GS::UniString            label;
    std::vector<PendingChange> buffer;
};

UndoState& GetUndoState()
{
    static UndoState us;
    return us;
}

} // anonymous namespace

bool ArchiLua::UndoIsActive()
{
    return GetUndoState().active;
}

void ArchiLua::UndoBegin(const char* label)
{
    auto& us = GetUndoState();
    for (auto& pc : us.buffer) {
        if (pc.hasMemo)
            ACAPI_DisposeElemMemoHdls(&pc.memo);
    }
    us.buffer.clear();
    us.active = true;
    us.label = label ? GS::UniString(label) : GS::UniString("ArchiLua operation");
}

GSErrCode ArchiLua::UndoEnd()
{
    auto& us = GetUndoState();
    if (!us.active || us.buffer.empty()) {
        us.active = false;
        return NoError;
    }

    GS::UniString savedLabel = us.label;
    std::vector<PendingChange> savedBuffer;
    savedBuffer.swap(us.buffer);
    us.active = false;

    GSErrCode err = ACAPI_CallUndoableCommand(savedLabel, [&]() -> GSErrCode {
        for (auto& pc : savedBuffer) {
            GSErrCode e = ACAPI_Element_Change(&pc.elem, &pc.mask, pc.hasMemo ? &pc.memo : nullptr, 0, true);
            if (e != NoError)
                return e;
        }
        return NoError;
    });

    for (auto& pc : savedBuffer) {
        if (pc.hasMemo)
            ACAPI_DisposeElemMemoHdls(&pc.memo);
    }
    return err;
}

void ArchiLua::UndoBuffer(const API_Guid& guid, const API_Element& elem, const API_Element& mask)
{
    auto& us = GetUndoState();
    for (auto& pc : us.buffer) {
        if (pc.guid == guid) {
            pc.elem = elem;
            pc.mask = mask;
            pc.hasMemo = false;
            return;
        }
    }
    us.buffer.push_back({guid, elem, mask, {}, false});
}
