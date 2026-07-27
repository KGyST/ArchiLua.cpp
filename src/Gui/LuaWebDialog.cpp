#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ArchiLua_Resource.h"
#include "ArchiLua.hpp"
#include "LuaWebDialog.hpp"
#include "../Bridge/LuaBridge.hpp"
#include "AC27/APICommon.h"

#include <string>
#include <cstdio>

namespace ArchiLua {

// ---------------------------------------------------------------------------
// Pick a wall (interactive)
// ---------------------------------------------------------------------------

static GS::UniString PickAWall()
{
    API_Guid guid = APINULLGuid;
    API_Coord3D clickPos;
    ACAPI_WriteReport("[DBG] PickAWall: before ClickAnElem", false);
    bool ok = ClickAnElem("Click a wall to select it", API_WallID,
                          nullptr, nullptr, &guid, &clickPos);
    GS::UniString guidStr = APIGuidToString(guid);
    char buf[256];
    sprintf_s(buf, "[DBG] PickAWall: ok=%d guid=%s", ok, guidStr.ToCStr().Get());
    ACAPI_WriteReport(buf, false);
    if (ok && guid != APINULLGuid)
        return guidStr;
    ACAPI_WriteReport("[DBG] PickAWall: returning empty", false);
    return {};
}

GS::UniString LuaWebDialog::DispatchUIEvent(const std::string& eventName)
{
    char buf[256];
    sprintf_s(buf, "[DBG] DispatchUIEvent: eventName=%s", eventName.c_str());
    ACAPI_WriteReport(buf, false);
    if (eventName == "onPickWall") {
        GS::UniString result = PickAWall();
        sprintf_s(buf, "[DBG] DispatchUIEvent: result=%s", result.ToCStr().Get());
        ACAPI_WriteReport(buf, false);
        return result;
    }
    ACAPI_WriteReport("[DBG] DispatchUIEvent: unknown event, returning empty", false);
    return {};
}

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

// ---------------------------------------------------------------------------
// Register the JS bridge
// ---------------------------------------------------------------------------

static void RegisterJSObject(DG::Browser& browser, LuaWebDialog* dlg)
{
    JS::Object* jsArchilua = new JS::Object("archilua");

    jsArchilua->AddItem(new JS::Function("DispatchEvent", [dlg, &browser] (GS::Ref<JS::Base> params) {
        ACAPI_WriteReport("[DBG] JS DispatchEvent: entered", false);
        GS::Ref<JS::Value> jsValue = GS::DynamicCast<JS::Value>(params);
        if (jsValue != nullptr && jsValue->GetType() == JS::Value::STRING) {
            GS::UniString eventName = jsValue->GetString();
            char buf[256];
            sprintf_s(buf, "[DBG] JS DispatchEvent: eventName=%s", eventName.ToCStr().Get());
            ACAPI_WriteReport(buf, false);
            GS::UniString result = dlg->DispatchUIEvent(std::string(eventName.ToCStr().Get()));
            sprintf_s(buf, "[DBG] JS DispatchEvent: result=%s", result.ToCStr().Get());
            ACAPI_WriteReport(buf, false);
            if (!result.IsEmpty()) {
                sprintf_s(buf, "document.getElementById('result').textContent='GUID: %s';", result.ToCStr().Get());
                browser.ExecuteJS(GS::UniString(buf));
            } else {
                browser.ExecuteJS("document.getElementById('result').textContent='Cancelled (pressed Escape or clicked empty space).';");
            }
        } else {
            ACAPI_WriteReport("[DBG] JS DispatchEvent: params is not STRING", false);
        }
        return GS::Ref<JS::Base>(new JS::Value());
    }));

    browser.RegisterAsynchJSObject(jsArchilua);
}

// ---------------------------------------------------------------------------
// HTML / JS for the web palette
// ---------------------------------------------------------------------------

static GS::UniString BuildHTML()
{
    return GS::UniString(
        L"<html><head><style>"
        L"body{background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:16px;margin:0;}"
        L"button{background:#0e639c;color:#fff;border:none;padding:8px 16px;font-size:14px;cursor:pointer;border-radius:3px;}"
        L"button:hover{background:#1177bb;}"
        L"#result{margin-top:12px;padding:8px;background:#2d2d2d;border-radius:3px;font-size:13px;white-space:pre-wrap;word-break:break-all;}"
        L"</style></head><body>"
        L"<button id='btnPick' onclick='pickWall()'>Pick Wall</button>"
        L"<div id='result'>Press button then click a wall in ArchiCAD.</div>"
        L"<script>"
        L"function pickWall(){"
        L"  var r=document.getElementById('result');"
        L"  r.textContent='Click a wall in the ArchiCAD viewport...';"
        L"  try{"
        L"    var guid=archilua.DispatchEvent('onPickWall');"
        L"    if(guid&&guid.length>0){"
        L"      r.textContent='GUID: '+guid;"
        L"    }else{"
        L"      r.textContent='Cancelled (pressed Escape or clicked empty space).';"
        L"    }"
        L"  }catch(e){"
        L"    r.textContent='Error: '+e.message;"
        L"  }"
        L"}"
        L"</script></body></html>"
    );
}

// --- Constructor / Destructor ---

LuaWebDialog::LuaWebDialog()
    : DG::Palette(ACAPI_GetOwnResModule(), LUA_WEB_DIALOG,
                  ACAPI_GetOwnResModule(), PaletteGuid())
    , browser(GetReference(), Browser_Web)
{
    Attach(*this);
    BeginEventProcessing();

    browser.LoadHTML(BuildHTML());
    RegisterJSObject(browser, this);
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
