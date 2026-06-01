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
            LuaScriptDialog dlg;
            dlg.Invoke();
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

    return err;
}

//------------------------------------------------------
// FreeData
//------------------------------------------------------
GSErrCode __ACENV_CALL FreeData(void)
{
    return NoError;
}
