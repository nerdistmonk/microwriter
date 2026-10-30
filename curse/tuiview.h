#ifndef MICROWRITER_TUIVIEW_H
#define MICROWRITER_TUIVIEW_H

#include "tuitypes.h"
#include <vector>
#include <string>

namespace TuiView {
    extern const std::vector<std::string> toolbarButtons;
    extern const std::vector<std::string> codeButtons;

    void render(
        const std::vector<EditorDocument> &documents,
        size_t activeDocIdx,
        bool menuActive,
        int menuSelect,
        const std::vector<std::string> &topMenus,
        bool saveFlash = false
    );
}

#endif // MICROWRITER_TUIVIEW_H
