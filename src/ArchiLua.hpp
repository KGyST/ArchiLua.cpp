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
    bool            isDelete = false;
};

void UndoBegin(const char* label);
GSErrCode UndoEnd();
bool UndoIsActive();
void UndoBuffer(const API_Guid& guid, const API_Element& elem, const API_Element& mask);
// Buffer a params-memo Change; takes ownership of memo handles.
void UndoBufferMemo(const API_Guid& guid, const API_Element& elem, const API_Element& mask, const API_ElementMemo& memo);
// Buffer a delete; replayed (one call for all) before Changes at UndoEnd.
void UndoBufferDelete(const API_Guid& guid);

} // namespace ArchiLua

#endif // !ARCHILUA_HPP
