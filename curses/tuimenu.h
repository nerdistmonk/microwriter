#ifndef MICROWRITER_TUIMENU_H
#define MICROWRITER_TUIMENU_H

#include "tuitypes.h"
#include <vector>
#include <string>
#include <functional>

namespace TuiMenu {
    void runMenuLoop(
        std::vector<EditorDocument> &documents,
        size_t &activeDocIdx,
        int &initialMenuSelect,
        const std::vector<std::string> &topMenus,
        const std::vector<std::vector<std::string>> &subMenus,
        std::function<void(int menuIdx, int itemIdx)> onAction
    );
}

#endif // MICROWRITER_TUIMENU_H
