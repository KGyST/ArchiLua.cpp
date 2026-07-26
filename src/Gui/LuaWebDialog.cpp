#include "APIEnvir.h"
#include "ACAPinc.h"
#include "ArchiLua_Resource.h"
#include "ArchiLua.hpp"
#include "LuaWebDialog.hpp"
#include "../Bridge/LuaBridge.hpp"

namespace ArchiLua {

// --- JS value helpers ---

static GS::UniString GetStringFromJSArg(GS::Ref<JS::Base> jsVariable)
{
    GS::Ref<JS::Value> jsValue = GS::DynamicCast<JS::Value>(jsVariable);
    if (DBVERIFY(jsValue != nullptr && jsValue->GetType() == JS::Value::STRING))
        return jsValue->GetString();
    return GS::EmptyUniString;
}

template<class Type>
static GS::Ref<JS::Base> ToJSValue(const Type& cppVariable)
{
    return new JS::Value(cppVariable);
}

static GS::Ref<JS::Base> GUIDsToJSArray(const GS::Array<API_Guid>& guids)
{
    GS::Ref<JS::Array> jsArr = new JS::Array();
    for (const API_Guid& guid : guids) {
        jsArr->AddItem(ToJSValue(APIGuidToString(guid)));
    }
    return jsArr;
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

static GS::Array<API_Guid> GetSelectedGUIDs()
{
    API_SelectionInfo selectionInfo;
    GS::Array<API_Neig> selNeigs;
    ACAPI_Selection_Get(&selectionInfo, &selNeigs, false, false);
    BMKillHandle((GSHandle*)&selectionInfo.marquee.coords);

    GS::Array<API_Guid> guids;
    for (const API_Neig& neig : selNeigs)
        guids.Push(neig.guid);
    return guids;
}

static void RegisterJSObject(DG::Browser& browser)
{
    JS::Object* jsArchilua = new JS::Object("archilua");

    jsArchilua->AddItem(new JS::Function("GetSelectedElements", [] (GS::Ref<JS::Base>) {
        return GUIDsToJSArray(GetSelectedGUIDs());
    }));

    browser.RegisterAsynchJSObject(jsArchilua);
}

// --- Constructor / Destructor ---

LuaWebDialog::LuaWebDialog()
    : DG::Palette(ACAPI_GetOwnResModule(), LUA_WEB_DIALOG,
                  ACAPI_GetOwnResModule(), PaletteGuid())
    , browser(GetReference(), Browser_Web)
{
    Attach(*this);
    BeginEventProcessing();

    browser.LoadHTML(GS::UniString(
        L"<html><head><style>"
        L"body{background:#1e1e1e;color:#ccc;font-family:Segoe UI,sans-serif;padding:16px;margin:0;}"
        L"button{background:#0e639c;color:#fff;border:none;padding:8px 16px;font-size:14px;cursor:pointer;border-radius:3px;}"
        L"button:hover{background:#1177bb;}"
        L"#result{margin-top:12px;padding:8px;background:#2d2d2d;border-radius:3px;font-size:13px;white-space:pre-wrap;word-break:break-all;}"
        L"</style></head><body>"
        L"<button id='btnPick' onclick='pickWall()'>Pick Selected Wall</button>"
        L"<div id='result'>Click the button with a wall selected.</div>"
        L"<script>"
        L"function pickWall(){"
        L"  var r=document.getElementById('result');"
        L"  r.textContent='Fetching...';"
        L"  try{"
        L"    var guids=archilua.GetSelectedElements();"
        L"    if(guids&&guids.length>0){"
        L"      r.textContent='GUID: '+guids[0];"
        L"    }else{"
        L"      r.textContent='No elements selected.';"
        L"    }"
        L"  }catch(e){"
        L"    r.textContent='Error: '+e.message;"
        L"  }"
        L"}"
        L"</script></body></html>"
    ));

    RegisterJSObject(browser);
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
