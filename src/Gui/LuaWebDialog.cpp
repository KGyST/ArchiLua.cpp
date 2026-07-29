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

static const char* kWebEventsKey = "ArchiLua_WebDialog_Events";

// ---------------------------------------------------------------------------
// Lua-callable: PickWall() — interactive wall pick
// ---------------------------------------------------------------------------

static int L_PickWall(lua_State* L)
{
    API_Guid guid = APINULLGuid;
    API_Coord3D clickPos;
    bool ok = ClickAnElem("Click a wall to select it", API_WallID,
                          nullptr, nullptr, &guid, &clickPos);
    if (ok && guid != APINULLGuid) {
        GS::UniString guidStr = APIGuidToString(guid);
        lua_pushstring(L, guidStr.ToCStr().Get());
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

// ---------------------------------------------------------------------------
// Lua-callable: RegisterWebEvent(name, callback)
// ---------------------------------------------------------------------------

static int L_RegisterWebEvent(lua_State* L)
{
    const char* name = lua_tostring(L, 1);
    if (!name || !lua_isfunction(L, 2))
        return luaL_error(L, "RegisterWebEvent: expected string and function");

    lua_getfield(L, LUA_REGISTRYINDEX, kWebEventsKey);
    lua_pushvalue(L, 2);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
    return 0;
}

// ---------------------------------------------------------------------------
// Lua-callable: SetWebResult(text) — update the HTML result div
// ---------------------------------------------------------------------------

static int L_SetWebResult(lua_State* L)
{
    const char* text = lua_tostring(L, 1);
    if (!text)
        return 0;

    auto& bridge = GetBridge();
    auto* dlg = static_cast<LuaWebDialog*>(bridge.GetDialog());
    if (dlg) {
        char buf[512];
        sprintf_s(buf, "document.getElementById('result').textContent='%s';", text);
        dlg->ExecuteJS(GS::UniString(buf));
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Lua-callable: ShowWebDialog() — create and show the web palette
// ---------------------------------------------------------------------------

static int L_ShowWebDialog(lua_State* L)
{
    auto& bridge = GetBridge();
    auto* existing = bridge.GetDialog();
    if (existing) {
        auto* webDlg = dynamic_cast<LuaWebDialog*>(existing);
        if (webDlg) {
            webDlg->Show();
            return 0;
        }
    }
    LuaWebDialog* dlg;
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        dlg = new LuaWebDialog(GS::UniString(lua_tostring(L, 1)));
    } else {
        dlg = new LuaWebDialog();
    }
    bridge.SetDialog(dlg);
    dlg->Show();
    return 0;
}

// ---------------------------------------------------------------------------
// Lua-callable: GetEventPayload() — returns JSON string from last JS send
// ---------------------------------------------------------------------------

static int L_GetEventPayload(lua_State* L)
{
    auto& bridge = GetBridge();
    auto* dlg = dynamic_cast<LuaWebDialog*>(bridge.GetDialog());
    if (dlg) {
        lua_pushstring(L, dlg->GetPayload().c_str());
    } else {
        lua_pushstring(L, "");
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Lua-callable: ExecuteJS(jsString) — run arbitrary JS in the browser
// ---------------------------------------------------------------------------

static int L_ExecuteJS(lua_State* L)
{
    const char* js = lua_tostring(L, 1);
    if (!js)
        return 0;
    auto& bridge = GetBridge();
    auto* dlg = dynamic_cast<LuaWebDialog*>(bridge.GetDialog());
    if (dlg)
        dlg->ExecuteJS(GS::UniString(js));
    return 0;
}

// ---------------------------------------------------------------------------
// Register Web UI Lua functions in the Lua state
// ---------------------------------------------------------------------------

void RegisterWebUIFunctions(lua_State* L)
{
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kWebEventsKey);

    lua_register(L, "RegisterWebEvent", L_RegisterWebEvent);
    lua_register(L, "SetWebResult", L_SetWebResult);
    lua_register(L, "PickWall", L_PickWall);
    lua_register(L, "ShowWebDialog", L_ShowWebDialog);
    lua_register(L, "GetEventPayload", L_GetEventPayload);
    lua_register(L, "ExecuteJS", L_ExecuteJS);
}

// ---------------------------------------------------------------------------
// DispatchUIEvent — route JS events to Lua callbacks
// ---------------------------------------------------------------------------

GS::UniString LuaWebDialog::DispatchUIEvent(const std::string& eventName)
{
    lua_State* L = GetBridge().State();
    if (!L)
        return {};

    auto& debugger = GetBridge().m_debugger;
    bool hadHook = false;
    if (debugger.HasClient()) {
        LuaDebugger::ActivateForCallback(&debugger);
        lua_sethook(L, LuaDebugger::DebugHook, LUA_MASKLINE, 0);
        hadHook = true;
    }

    lua_pushcfunction(L, [](lua_State* L2) -> int {
        const char* name = lua_tostring(L2, 1);
        if (!name)
            return 0;
        lua_getfield(L2, LUA_REGISTRYINDEX, kWebEventsKey);
        if (!lua_istable(L2, -1)) {
            lua_pop(L2, 1);
            return 0;
        }
        lua_getfield(L2, -1, name);
        if (lua_isfunction(L2, -1)) {
            if (lua_pcall(L2, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L2, -1);
                if (err) ACAPI_WriteReport(err, true);
                lua_pop(L2, 1);
                lua_pop(L2, 1);
                return 0;
            }
            lua_pop(L2, 1);
            return 0;
        }
        lua_pop(L2, 2);
        return 0;
    });
    lua_pushstring(L, eventName.c_str());
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        if (err)
            ACAPI_WriteReport(err, true);
        lua_pop(L, 1);
    }

    if (hadHook) {
        lua_sethook(L, nullptr, 0, 0);
        LuaDebugger::DeactivateForCallback();
    }
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
    auto* dlg = dynamic_cast<LuaWebDialog*>(bridge.GetDialog());
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

    jsArchilua->AddItem(new JS::Function("DispatchEvent", [dlg] (GS::Ref<JS::Base> params) {
        GS::Ref<JS::Value> jsValue = GS::DynamicCast<JS::Value>(params);
        if (jsValue != nullptr && jsValue->GetType() == JS::Value::STRING) {
            GS::UniString eventName = jsValue->GetString();
            dlg->DispatchUIEvent(std::string(eventName.ToCStr().Get()));
        }
        return GS::Ref<JS::Base>(new JS::Value());
    }));

    // SendEvent(name, payload) — like DispatchEvent but with a payload string
    jsArchilua->AddItem(new JS::Function("SendEvent", [dlg] (GS::Ref<JS::Base> params) {
        GS::Ref<JS::Array> arr = GS::DynamicCast<JS::Array>(params);
        if (arr != nullptr) {
            const auto& items = arr->GetItemArray();
            if (items.GetSize() >= 1) {
                GS::UniString eventName;
                std::string payload;
                GS::Ref<JS::Value> nameVal = GS::DynamicCast<JS::Value>(items[0]);
                if (nameVal != nullptr && nameVal->GetType() == JS::Value::STRING)
                    eventName = nameVal->GetString();
                if (items.GetSize() >= 2) {
                    GS::Ref<JS::Value> payloadVal = GS::DynamicCast<JS::Value>(items[1]);
                    if (payloadVal != nullptr && payloadVal->GetType() == JS::Value::STRING)
                        payload = std::string(payloadVal->GetString().ToCStr().Get());
                }
                dlg->SetPayload(payload);
                dlg->DispatchUIEvent(std::string(eventName.ToCStr().Get()));
            }
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
        L"  document.getElementById('result').textContent='Click a wall in the ArchiCAD viewport...';"
        L"  archilua.DispatchEvent('onPickWall');"
        L"}"
        L"</script></body></html>"
    );
}

// --- Constructor / Destructor ---

LuaWebDialog::LuaWebDialog()
    : LuaWebDialog(BuildHTML())
{
}

LuaWebDialog::LuaWebDialog(const GS::UniString& html)
    : DG::Palette(ACAPI_GetOwnResModule(), LUA_WEB_DIALOG,
                  ACAPI_GetOwnResModule(), PaletteGuid())
    , browser(GetReference(), Browser_Web)
{
    Attach(*this);
    BeginEventProcessing();

    browser.LoadHTML(html);
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