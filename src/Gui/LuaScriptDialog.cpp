#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ArchiLua_Resource.h"
#include "ArchiLua.hpp"
#include "LuaScriptDialog.hpp"
#include "../Bridge/LuaBridge.hpp"

namespace ArchiLua {

const GS::Guid& LuaScriptDialog::PaletteGuid()
{
    static GS::Guid guid("5E8C4B3A-1F2D-4A7E-9B6C-3D8E1F2A4B5C");
    return guid;
}

Int32 LuaScriptDialog::PaletteRefId()
{
    static Int32 refId(GS::CalculateHashValue(PaletteGuid()));
    return refId;
}

GSErrCode __ACENV_CALL LuaScriptDialog::PaletteAPIControlCallBack(Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param)
{
    if (referenceID == PaletteRefId()) {
        auto& bridge = GetBridge();
        auto* dlg = bridge.GetDialog();
        switch (messageID) {
            case APIPalMsg_ClosePalette:
                if (dlg && dlg->IsVisible())
                    dlg->SendCloseRequest();
                break;
            case APIPalMsg_HidePalette_Begin:
                if (dlg && dlg->IsVisible())
                    dlg->Hide();
                break;
            case APIPalMsg_HidePalette_End:
            case APIPalMsg_OpenPalette:
                if (dlg && !dlg->IsVisible())
                    dlg->Show();
                break;
            case APIPalMsg_DisableItems_Begin:
                break;
            case APIPalMsg_DisableItems_End:
                break;
            case APIPalMsg_IsPaletteVisible:
                if (param)
                    *reinterpret_cast<bool*>(param) = dlg && dlg->IsVisible();
                break;
        }
    }
    return NoError;
}

LuaScriptDialog::LuaScriptDialog()
    : DG::Palette(ACAPI_GetOwnResModule(), LUA_RUNNER_DIALOG, ACAPI_GetOwnResModule(), PaletteGuid())
    , runButton(GetReference(), DG_OK)
    , scriptPathEdit(GetReference(), TextEdit_ScriptPath)
    , browseButton(GetReference(), Button_Browse)
{
    Attach(*this);
    AttachToAllItems(*this);

    const std::string& lastPath = GetBridge().GetLastScriptPath();
    if (!lastPath.empty())
        scriptPathEdit.SetText(GS::UniString(lastPath.c_str()));

    BeginEventProcessing();
}

LuaScriptDialog::~LuaScriptDialog()
{
    EndEventProcessing();
}

void LuaScriptDialog::PanelCloseRequested(const DG::PanelCloseRequestEvent& ev, bool* accepted)
{
    Hide();
    *accepted = true;
}

void LuaScriptDialog::PanelClosed(const DG::PanelCloseEvent& ev)
{
    if (GetBridge().GetDialog() == this)
        GetBridge().SetDialog(nullptr);
    delete this;
}

void LuaScriptDialog::ButtonClicked(const DG::ButtonClickEvent& ev)
{
    switch (ev.GetSource()->GetId()) {
        case DG_CANCEL:
            SendCloseRequest();
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
