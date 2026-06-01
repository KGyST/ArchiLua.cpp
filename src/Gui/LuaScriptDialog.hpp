#pragma once

#include "DGModule.hpp"

namespace ArchiLua {

class LuaScriptDialog :
    public DG::ModalDialog,
    public DG::ButtonItemObserver,
    public DG::PanelObserver,
    public DG::CompoundItemObserver
{
private:
  DG::Button      runButton;
  DG::Button      cancelButton;
  DG::TextEdit    scriptPathEdit;
  DG::Button      browseButton;

public:
    LuaScriptDialog();
    ~LuaScriptDialog();

    void ButtonClicked(const DG::ButtonClickEvent& ev) override;
    void PanelCloseRequested(const DG::PanelCloseRequestEvent& ev, bool* accepted) override;
};

} // namespace ArchiLua
