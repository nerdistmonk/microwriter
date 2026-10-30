#ifndef TUIINPUT_H
#define TUIINPUT_H

#include "tuitypes.h"
#include <curses.h>
#include <functional>
#include <vector>
#include <string>
#include <chrono>

struct TuiEditorContext {
    std::vector<EditorDocument> &documents;
    size_t &activeDocIdx;
    bool &running;
    bool &isDragging;
    bool &saveFlashActive;
    bool defaultCodeMode;

    int &lastClickTabIdx;
    std::chrono::steady_clock::time_point &lastClickTime;

    const std::vector<std::string> &topMenus;
    const std::vector<std::vector<std::string>> &subMenus;

    std::function<void()> saveCurrentFile;
    std::function<void()> saveAsFile;
    std::function<void()> promptOpen;
    std::function<void(size_t)> closeDocument;
    std::function<void()> promptQuit;
    std::function<void(TuiMode)> switchDocumentMode;
    std::function<void(const std::string&, const std::string&)> exportDocument;
    std::function<void(int, int)> executeMenuAction;

    EditorDocument& doc() { return documents[activeDocIdx]; }
};

namespace TuiInput {

void handleMouseEvent(const MEVENT &event, TuiEditorContext &ctx);
void handleInput(int ch, TuiEditorContext &ctx);

} // namespace TuiInput

#endif // TUIINPUT_H
