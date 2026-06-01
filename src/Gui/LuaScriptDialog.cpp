#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ArchiLua_Resource.h"
#include "ArchiLua.hpp"
#include "LuaScriptDialog.hpp"
#include "../Bridge/LuaBridge.hpp"

namespace ArchiLua {

LuaScriptDialog::LuaScriptDialog()
    : DG::ModalDialog(ACAPI_GetOwnResModule(), LUA_RUNNER_DIALOG, ACAPI_GetOwnResModule())
    , runButton(GetReference(), DG_OK)
    , cancelButton(GetReference(), DG_CANCEL)
    , scriptPathEdit(GetReference(), TextEdit_ScriptPath)
    , browseButton(GetReference(), Button_Browse)
{
    Attach(*this);
    AttachToAllItems(*this);

    const std::string& lastPath = GetBridge().GetLastScriptPath();
    if (!lastPath.empty())
        scriptPathEdit.SetText(GS::UniString(lastPath.c_str()));
}

LuaScriptDialog::~LuaScriptDialog()
{
}

void LuaScriptDialog::PanelCloseRequested(const DG::PanelCloseRequestEvent& ev, bool* accepted)
{
    *accepted = true;
}

void LuaScriptDialog::ButtonClicked(const DG::ButtonClickEvent& ev)
{
    switch (ev.GetSource()->GetId()) {
        case DG_CANCEL:
            PostCloseRequest(DG::ModalDialog::Cancel);
            break;

        case Button_Browse:
        {
            FTM::FileTypeManager ftman("ArchiLua");
            FTM::FileType luaFileType("Lua Scripts", "lua", 0, 0, 0);
            FTM::TypeID luaType = FTM::FileTypeManager::SearchForType(luaFileType);
            if (luaType == FTM::UnknownType)
                luaType = ftman.AddType(luaFileType);

            DG::FileDialog fileDlg(DG::FileDialog::OpenFile);
            fileDlg.AddFilter(luaType, DG::FileDialog::DisplayExtensions);
            if (fileDlg.Invoke()) {
                const IO::Location& sel = fileDlg.GetSelectedFile();
                GS::UniString pathStr;
                sel.ToPath(&pathStr);
                scriptPathEdit.SetText(pathStr);
            }
            break;
        }

        case DG_OK:
        {
            GS::UniString path = scriptPathEdit.GetText();
            if (path.IsEmpty()) {
                ACAPI_WriteReport("No script file selected.", true);
            } else {
                GetBridge().ExecuteScript(path.ToCStr().Get());
            }
            break;
        }
    }
}

} // namespace ArchiLua
