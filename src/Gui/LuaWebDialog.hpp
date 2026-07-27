#pragma once

#include "DGModule.hpp"
#include "DGBrowser.hpp"
#include "APIdefs_Interface.h"

#include <string>

namespace ArchiLua {

class LuaWebDialog :
    public DG::Palette,
    public DG::PanelObserver
{
private:
    DG::Browser browser;

public:
    GS::UniString DispatchUIEvent(const std::string& eventName);

public:
    LuaWebDialog();
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
