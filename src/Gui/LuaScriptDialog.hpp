#pragma once

#include "DGModule.hpp"

namespace ArchiLua {

class LuaScriptDialog :
    public DG::ModalDialog,
    public DG::ButtonItemObserver,
    public DG::CompoundItemObserver
{
private:
    DG::Button      closeButton;
    DG::TextEdit    scriptPathEdit;
    DG::Button      browseButton;
    DG::Button      runButton;

public:
    LuaScriptDialog();
    ~LuaScriptDialog();

    void ButtonClicked(const DG::ButtonClickEvent& ev) override;
};

} // namespace ArchiLua
