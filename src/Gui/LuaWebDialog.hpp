#pragma once

#include "DGModule.hpp"
#include "DGBrowser.hpp"
#include "APIdefs_Interface.h"

#include <string>

struct lua_State;

namespace ArchiLua {

void RegisterWebUIFunctions(lua_State* L);

class LuaWebDialog :
    public DG::Palette,
    public DG::PanelObserver
{
private:
    DG::Browser browser;
    std::string m_payload;

public:
    void ExecuteJS(const GS::UniString& js) { browser.ExecuteJS(js); }
    GS::UniString DispatchUIEvent(const std::string& eventName);
    void SetPayload(const std::string& p) { m_payload = p; }
    const std::string& GetPayload() const { return m_payload; }

public:
    LuaWebDialog();
    explicit LuaWebDialog(const GS::UniString& html);
    ~LuaWebDialog();

    static const GS::Guid& PaletteGuid();
    static Int32 PaletteRefId();
    static GSErrCode __ACENV_CALL PaletteAPIControlCallBack(
        Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param);

    void PanelResized(const DG::PanelResizeEvent& ev) override;
    void PanelCloseRequested(const DG::PanelCloseRequestEvent& ev, bool* accepted) override;
    void PanelClosed(const DG::PanelCloseEvent& ev) override;
};

} // namespace ArchiLua
