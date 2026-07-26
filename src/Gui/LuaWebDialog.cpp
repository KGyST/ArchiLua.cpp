#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ArchiLua_Resource.h"
#include "ArchiLua.hpp"
#include "LuaWebDialog.hpp"
#include "../Bridge/LuaBridge.hpp"

namespace ArchiLua {

// --- Static GUID / RefId ---

const GS::Guid& LuaWebDialog::PaletteGuid()
{
    static GS::Guid guid("E5D4C3B2-A1F0-9876-5432-10ABCDEFABCD");
    return guid;
}

Int32 LuaWebDialog::PaletteRefId()
{
    static Int32 refId(GS::CalculateHashValue(PaletteGuid()));
    return refId;
}

// --- API Callback ---

GSErrCode __ACENV_CALL LuaWebDialog::PaletteAPIControlCallBack(
    Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param)
{
    if (referenceID == PaletteRefId()) {
        auto& bridge = GetBridge();
        auto* dlg = static_cast<LuaWebDialog*>(bridge.GetDialog());
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

// --- Constructor / Destructor ---

LuaWebDialog::LuaWebDialog()
    : DG::Palette(ACAPI_GetOwnResModule(), LUA_WEB_DIALOG,
                  ACAPI_GetOwnResModule(), PaletteGuid())
    , browser(GetReference(), Browser_Web)
{
    Attach(*this);
    BeginEventProcessing();

    browser.LoadHTML(
        GS::UniString(L"<html><body style='background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:2em;'>"
        L"<h2>ArchiLua Web GUI</h2>"
        L"<p>WebView ready.</p>"
        L"</body></html>")
    );
}

LuaWebDialog::~LuaWebDialog()
{
    EndEventProcessing();
}

// --- Lifecycle ---
void LuaWebDialog::PanelCloseRequested(const DG::PanelCloseRequestEvent& ev, bool* accepted)
{
    Hide();
    *accepted = true;
}

void LuaWebDialog::PanelClosed(const DG::PanelCloseEvent& ev)
{
    GetBridge().SetDialog(nullptr);
    delete this;
}

void LuaWebDialog::PanelResized(const DG::PanelResizeEvent& ev)
{
    BeginMoveResizeItems();
    browser.Resize(ev.GetHorizontalChange(), ev.GetVerticalChange());
    EndMoveResizeItems();
}

} // namespace ArchiLua
