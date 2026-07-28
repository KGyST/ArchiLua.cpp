#pragma once

#include "DGModule.hpp"
#include "APIdefs_Interface.h"

namespace ArchiLua {

class LuaScriptDialog :
    public DG::Palette,
    public DG::ButtonItemObserver,
    public DG::PanelObserver,
    public DG::CompoundItemObserver
{
private:
  DG::Button      runButton;
  DG::TextEdit    scriptPathEdit;
  DG::Button      browseButton;

public:
    LuaScriptDialog();
    ~LuaScriptDialog();

    static const GS::Guid& PaletteGuid();
    static Int32 PaletteRefId();
    static GSErrCode __ACENV_CALL PaletteAPIControlCallBack(Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param);

    void ButtonClicked(const DG::ButtonClickEvent& ev) override;
    void PanelCloseRequested(const DG::PanelCloseRequestEvent& ev, bool* accepted) override;
    void PanelClosed(const DG::PanelCloseEvent& ev) override;
};

} // namespace ArchiLua
