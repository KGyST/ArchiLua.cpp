#ifndef ARCHILUA_HPP
#define ARCHILUA_HPP

#include "ACAPinc.h"
#include <vector>

namespace ArchiLua {

class Bridge;
Bridge& GetBridge();

struct PendingChange {
    API_Guid        guid;
    API_Element     elem;
    API_Element     mask;
    API_ElementMemo memo;
    bool            hasMemo = false;
};

void UndoBegin(const char* label);
GSErrCode UndoEnd();
bool UndoIsActive();
void UndoBuffer(const API_Guid& guid, const API_Element& elem, const API_Element& mask);

} // namespace ArchiLua

#endif // !ARCHILUA_HPP
