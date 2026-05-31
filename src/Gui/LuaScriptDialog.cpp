#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ArchiLua_Resource.h"
#include "ArchiLua.hpp"
#include "LuaScriptDialog.hpp"
#include "../Bridge/LuaBridge.hpp"

namespace ArchiLua {

LuaScriptDialog::LuaScriptDialog()
    : DG::ModalDialog(ACAPI_GetOwnResModule(), LUA_RUNNER_DIALOG, ACAPI_GetOwnResModule())
    , closeButton(GetReference(), DG_OK)
    , scriptPathEdit(GetReference(), TextEdit_ScriptPath)
    , browseButton(GetReference(), Button_Browse)
    , runButton(GetReference(), Button_Run)
{
    AttachToAllItems(*this);
    scriptPathEdit.SetText("lua_scripts\\try_hello.lua");
}

LuaScriptDialog::~LuaScriptDialog()
{
}

void LuaScriptDialog::ButtonClicked(const DG::ButtonClickEvent& ev)
{
    if (ev.GetSource() == &closeButton) {
        PostCloseRequest(DG::ModalDialog::Accept);
    } else if (ev.GetSource() == &browseButton) {
        DG::FileDialog fileDlg(DG::FileDialog::OpenFile);
        if (fileDlg.Invoke()) {
            const IO::Location& sel = fileDlg.GetSelectedFile();
            GS::UniString pathStr;
            sel.ToPath(&pathStr);
            scriptPathEdit.SetText(pathStr);
        }
    } else if (ev.GetSource() == &runButton) {
        GS::UniString path = scriptPathEdit.GetText();
        GetBridge().ExecuteScript(path.ToCStr().Get());
    }
}

} // namespace ArchiLua
