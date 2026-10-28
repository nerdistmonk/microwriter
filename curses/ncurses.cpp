#include "ncurses.h"
#include "tuitypes.h"
#include "tuidialogs.h"
#include "tuiactions.h"
#include "tuiview.h"
#include "tuimenu.h"

#include <curses.h>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>

class NcursesEditor {
public:
    NcursesEditor() = default;

    int run(const std::vector<std::string> &initialFiles, bool forceCodeMode = false) {
        m_defaultCodeMode = forceCodeMode;

        initscr();
        raw();
        noecho();
        keypad(stdscr, TRUE);

        printf("\033[?1000h\033[?1002h\033[?1006h");
        fflush(stdout);

        mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
        mouseinterval(0);

        start_color();
        use_default_colors();

        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        init_pair(2, COLOR_BLACK, COLOR_CYAN);
        init_pair(3, COLOR_YELLOW, -1);
        init_pair(4, COLOR_CYAN, -1);
        init_pair(5, COLOR_BLACK, COLOR_WHITE);
        init_pair(6, COLOR_WHITE, COLOR_BLACK);

        if (initialFiles.empty()) {
            EditorDocument d;
            if (m_defaultCodeMode) d.mode = TuiMode::Code;
            m_documents.push_back(d);
        } else {
            for (const auto &file : initialFiles) loadFile(file);
        }

        m_activeDocIdx = 0;
        m_running = true;

        while (m_running) {
            updateScroll();
            TuiView::render(m_documents, m_activeDocIdx, false, 0, m_topMenus, m_saveFlashActive);
            if (m_saveFlashActive) {
                m_saveFlashActive = false; // Reset flash state after render
            }
            int ch = getch();
            handleInput(ch);
        }

        printf("\033[?1000l\033[?1002l\033[?1006l");
        fflush(stdout);
        endwin();
        return 0;
    }

private:
    std::vector<EditorDocument> m_documents;
    size_t m_activeDocIdx = 0;
    bool m_running = true;
    bool m_isDragging = false;
    bool m_defaultCodeMode = false;
    bool m_saveFlashActive = false;

    // For tab double click detection
    int m_lastClickTabIdx = -1;
    std::chrono::steady_clock::time_point m_lastClickTime;

    const std::vector<std::string> m_topMenus = {"File", "Edit", "Format", "Tools", "Help"};
    const std::vector<std::vector<std::string>> m_subMenus = {
        {
            "New", "Open", "Save", "Save As", "Switch Mode",
            "Encoding: UTF-8", "Encoding: ANSI (Windows-1252)", "Encoding: ISO-8859-1",
            "EOL: Unix (LF)", "EOL: Windows (CRLF)", "EOL: Mac (CR)",
            "Exit"
        },
        {"Undo", "Redo", "Cut", "Copy", "Paste", "Duplicate Line/Block", "Find & Replace"},
        {"Select Font", "Font Size", "Text Color", "Bold", "Italic", "Underline", "Blockquote", "Insert Table", "Horizontal Line"},
        {"Toggle Comment //", "Move Line Up", "Move Line Down", "Indent", "Unindent", "Strip Blank Lines", "Sort Lines", "Reverse Lines", "To Uppercase", "To Lowercase"},
        {"About MicroWriter TUI", "Regex Cheat Sheet", "HTML Quick Reference", "Sed & Utilities Cheat Sheet", "Useful Bash Scripts"}
    };

    EditorDocument& doc() { return m_documents[m_activeDocIdx]; }

    void updateScroll() {
        int contentTop = 3;
        int contentHeight = LINES - contentTop - 1;
        if (doc().cursor.line < doc().scrollLine) doc().cursor.line = doc().scrollLine;
        if (doc().cursor.line >= doc().scrollLine + contentHeight) doc().scrollLine = doc().cursor.line - contentHeight + 1;
    }

    void loadFile(const std::string &path) {
        EditorDocument d;
        d.filePath = path;
        size_t lastSlash = path.find_last_of("/\\");
        d.title = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

        std::string ext = "";
        size_t dot = d.title.find_last_of('.');
        if (dot != std::string::npos) {
            ext = d.title.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        static const std::vector<std::string> codeExts = {
            "cpp", "c", "h", "hpp", "cc", "cxx", "py", "js", "ts", "json", "css", "xml", "sh", "cmake"
        };
        if (m_defaultCodeMode || std::find(codeExts.begin(), codeExts.end(), ext) != codeExts.end()) {
            d.mode = TuiMode::Code;
        } else {
            d.mode = TuiMode::RichText;
        }

        std::ifstream file(path);
        if (file.is_open()) {
            d.lines.clear();
            std::string rawContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (rawContent.find("\r\n") != std::string::npos) d.eolFormat = "CRLF";

            if (d.mode == TuiMode::RichText && (ext == "html" || ext == "htm") && rawContent.find("<p") != std::string::npos) {
                size_t pos = 0;
                while ((pos = rawContent.find("<p", pos)) != std::string::npos) {
                    size_t closeP = rawContent.find(">", pos);
                    if (closeP == std::string::npos) break;
                    size_t endP = rawContent.find("</p>", closeP);
                    if (endP == std::string::npos) break;

                    std::string lineContent = rawContent.substr(closeP + 1, endP - closeP - 1);
                    if (lineContent == "<br />" || lineContent == "<br>" || lineContent == "<br/>") {
                        d.lines.push_back("");
                    } else {
                        d.lines.push_back(lineContent);
                    }
                    pos = endP + 4;
                }
            } else {
                std::stringstream ss(rawContent);
                std::string line;
                while (std::getline(ss, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    d.lines.push_back(line);
                }
            }
            if (d.lines.empty()) d.lines.push_back("");
        }

        m_documents.push_back(d);
    }

    void triggerSaveFlash() {
        m_saveFlashActive = true;
        TuiView::render(m_documents, m_activeDocIdx, false, 0, m_topMenus, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        m_saveFlashActive = false;
        TuiView::render(m_documents, m_activeDocIdx, false, 0, m_topMenus, false);
    }

    void saveCurrentFile() {
        if (doc().filePath.empty()) {
            std::string path = TuiDialogs::showPromptModal("Save As File: ");
            if (path.empty()) return;
            doc().filePath = path;
            size_t lastSlash = path.find_last_of("/\\");
            doc().title = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
        }

        std::ofstream out(doc().filePath, std::ios::binary);
        if (!out.is_open()) return;

        std::string ext = "";
        size_t dot = doc().filePath.find_last_of('.');
        if (dot != std::string::npos) {
            ext = doc().filePath.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        if (doc().mode == TuiMode::RichText && (ext == "html" || ext == "htm")) {
            bool hasHtmlRoot = false;
            for (const auto &l : doc().lines) {
                if (l.find("<html") != std::string::npos || l.find("<!DOCTYPE") != std::string::npos) {
                    hasHtmlRoot = true;
                    break;
                }
            }

            if (!hasHtmlRoot) {
                out << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\" />\n";
                out << "<style type=\"text/css\">\n";
                out << "p { margin: 0px; white-space: pre-wrap; }\n";
                out << "</style></head><body>\n";

                for (const auto &l : doc().lines) {
                    if (l.empty()) {
                        out << "<p><br /></p>\n";
                    } else if (l.find("<table") != std::string::npos || l.find("<hr") != std::string::npos ||
                               l.find("<tr") != std::string::npos || l.find("<td") != std::string::npos) {
                        out << l << "\n";
                    } else {
                        out << "<p>" << l << "</p>\n";
                    }
                }
                out << "</body></html>\n";
                doc().modified = false;
                triggerSaveFlash();
                return;
            }
        }

        for (size_t i = 0; i < doc().lines.size(); ++i) {
            out << doc().lines[i];
            if (i + 1 < doc().lines.size()) out << (doc().eolFormat == "CRLF" ? "\r\n" : (doc().eolFormat == "CR" ? "\r" : "\n"));
        }
        doc().modified = false;
        triggerSaveFlash();
    }

    void exportDocument(const std::string &extLabel, const std::string &defaultExt) {
        std::string defName = doc().filePath.empty() ? ("untitled." + defaultExt) : doc().title + "." + defaultExt;
        std::string target = TuiDialogs::showPromptModal("Export to " + extLabel + " (" + defName + "): ");
        if (target.empty()) target = defName;

        std::ofstream out(target, std::ios::binary);
        if (out.is_open()) {
            for (size_t i = 0; i < doc().lines.size(); ++i) {
                out << doc().lines[i];
                if (i + 1 < doc().lines.size()) out << (doc().eolFormat == "CRLF" ? "\r\n" : (doc().eolFormat == "CR" ? "\r" : "\n"));
            }
        }
    }

    void promptOpen() {
        std::string p = TuiDialogs::showPromptModal("Open File: ");
        if (!p.empty()) {
            loadFile(p);
            m_activeDocIdx = m_documents.size() - 1;
        }
    }

    void closeDocument(size_t index) {
        if (index >= m_documents.size()) return;
        if (m_documents[index].modified) {
            std::string res = TuiDialogs::showPromptModal("Unsaved changes in " + m_documents[index].title + ". Save first? (y/n/c): ");
            if (res == "y" || res == "Y") {
                size_t oldActive = m_activeDocIdx;
                m_activeDocIdx = index;
                saveCurrentFile();
                m_activeDocIdx = oldActive;
            } else if (res != "n" && res != "N") {
                return; // Cancelled
            }
        }
        m_documents.erase(m_documents.begin() + index);
        if (m_documents.empty()) {
            EditorDocument d;
            if (m_defaultCodeMode) d.mode = TuiMode::Code;
            m_documents.push_back(d);
            m_activeDocIdx = 0;
        } else if (m_activeDocIdx >= m_documents.size()) {
            m_activeDocIdx = m_documents.size() - 1;
        }
    }

    void promptQuit() {
        bool anyMod = false;
        for (const auto &d : m_documents) {
            if (d.modified) { anyMod = true; break; }
        }
        if (!anyMod) {
            m_running = false;
            return;
        }
        std::string res = TuiDialogs::showPromptModal("Unsaved changes. Save before quit? (y/n/c): ");
        if (res == "y" || res == "Y") {
            saveCurrentFile();
            m_running = false;
        } else if (res == "n" || res == "N") {
            m_running = false;
        }
    }

    void cutEntireLine() {
        doc().pushUndo();
        TuiActions::handleCutCopy(doc(), true);
        doc().modified = true;
    }

    void executeMenuAction(int menuIdx, int itemIdx) {
        if (menuIdx == 0) { // File
            if (itemIdx == 0) {
                EditorDocument d;
                if (m_defaultCodeMode) d.mode = TuiMode::Code;
                m_documents.push_back(d);
                m_activeDocIdx = m_documents.size() - 1;
            }
            else if (itemIdx == 1) promptOpen();
            else if (itemIdx == 2) saveCurrentFile();
            else if (itemIdx == 3) { doc().filePath = ""; saveCurrentFile(); }
            else if (itemIdx == 4) doc().mode = (doc().mode == TuiMode::RichText) ? TuiMode::Code : TuiMode::RichText;
            else if (itemIdx == 5) { doc().encoding = "UTF-8"; doc().modified = true; }
            else if (itemIdx == 6) { doc().encoding = "Windows-1252"; doc().modified = true; }
            else if (itemIdx == 7) { doc().encoding = "ISO-8859-1"; doc().modified = true; }
            else if (itemIdx == 8) { doc().eolFormat = "LF"; doc().modified = true; }
            else if (itemIdx == 9) { doc().eolFormat = "CRLF"; doc().modified = true; }
            else if (itemIdx == 10) { doc().eolFormat = "CR"; doc().modified = true; }
            else if (itemIdx == 11) promptQuit();
        } else if (menuIdx == 1) { // Edit
            if (itemIdx == 0) TuiActions::handleUndo(doc());
            else if (itemIdx == 1) TuiActions::handleRedo(doc());
            else if (itemIdx == 2) TuiActions::handleCutCopy(doc(), true);
            else if (itemIdx == 3) TuiActions::handleCutCopy(doc(), false);
            else if (itemIdx == 4) TuiActions::handlePaste(doc());
            else if (itemIdx == 5) TuiActions::handleDuplicate(doc());
            else if (itemIdx == 6) TuiActions::handleFindReplace(doc());
        } else if (menuIdx == 2) { // Format
            if (doc().mode != TuiMode::RichText) return;
            if (itemIdx == 0) TuiActions::handleFontSelection(doc());
            else if (itemIdx == 1) TuiActions::handleFontSizeSelection(doc());
            else if (itemIdx == 2) TuiActions::handleColorSelection(doc());
            else if (itemIdx == 3) TuiActions::injectTagAroundSelection(doc(), "<b>", "</b>");
            else if (itemIdx == 4) TuiActions::injectTagAroundSelection(doc(), "<i>", "</i>");
            else if (itemIdx == 5) TuiActions::injectTagAroundSelection(doc(), "<u>", "</u>");
            else if (itemIdx == 6) TuiActions::injectTagAroundSelection(doc(), "<blockquote>", "</blockquote>");
            else if (itemIdx == 7) TuiActions::handleInsertTable(doc());
            else if (itemIdx == 8) {
                doc().pushUndo();
                doc().lines[doc().cursor.line].insert(doc().cursor.col, "<hr>");
                doc().modified = true;
            }
        } else if (menuIdx == 3) { // Tools
            if (itemIdx == 0) TuiActions::handleToggleComment(doc());
            else if (itemIdx == 1) TuiActions::handleMoveLine(doc(), true);
            else if (itemIdx == 2) TuiActions::handleMoveLine(doc(), false);
            else if (itemIdx == 3) TuiActions::handleIndent(doc(), true);
            else if (itemIdx == 4) TuiActions::handleIndent(doc(), false);
            else if (itemIdx == 5) TuiActions::handleStripBlankLines(doc());
            else if (itemIdx == 6) TuiActions::handleSortLines(doc());
            else if (itemIdx == 7) TuiActions::handleReverseLines(doc());
            else if (itemIdx == 8) TuiActions::handleChangeCase(doc(), true);
            else if (itemIdx == 9) TuiActions::handleChangeCase(doc(), false);
        } else if (menuIdx == 4) { // Help
            if (itemIdx == 0) {
                TuiDialogs::showAboutModal();
            } else if (itemIdx == 1) {
                std::string findP, repP;
                if (TuiDialogs::showRegexCheatSheetModal(findP, repP) >= 0) {
                    TuiActions::handleFindReplace(doc(), findP, repP);
                }
            } else if (itemIdx == 2) {
                std::string tag;
                if (TuiDialogs::showHtmlCheatSheetModal(tag) >= 0) {
                    doc().pushUndo();
                    doc().lines[doc().cursor.line].insert(doc().cursor.col, tag);
                    doc().cursor.col += static_cast<int>(tag.length());
                    doc().modified = true;
                }
            } else if (itemIdx == 3) {
                std::string cmd;
                if (TuiDialogs::showSedUtilsCheatSheetModal(cmd) >= 0) {
                    doc().pushUndo();
                    doc().lines[doc().cursor.line].insert(doc().cursor.col, cmd);
                    doc().cursor.col += static_cast<int>(cmd.length());
                    doc().modified = true;
                }
            } else if (itemIdx == 4) {
                std::string script;
                if (TuiDialogs::showBashScriptCheatSheetModal(script) >= 0) {
                    doc().pushUndo();
                    std::stringstream ss(script);
                    std::string line;
                    std::vector<std::string> newLines;
                    while (std::getline(ss, line)) newLines.push_back(line);

                    if (!newLines.empty()) {
                        std::string cur = doc().lines[doc().cursor.line];
                        std::string prefix = cur.substr(0, doc().cursor.col);
                        std::string suffix = cur.substr(doc().cursor.col);

                        doc().lines[doc().cursor.line] = prefix + newLines[0];
                        for (size_t i = 1; i < newLines.size(); ++i) {
                            doc().lines.insert(doc().lines.begin() + doc().cursor.line + i, newLines[i]);
                        }
                        doc().cursor.line += static_cast<int>(newLines.size() - 1);
                        doc().lines[doc().cursor.line] += suffix;
                        doc().cursor.col = static_cast<int>(newLines.back().length());
                        doc().modified = true;
                    }
                }
            }
        }
    }

    void handleMouseEvent(const MEVENT &event) {
        int my = event.y, mx = event.x;

        // Row 0: Menu Bar
        if (my == 0 && (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
            int offset = 2;
            for (size_t i = 0; i < m_topMenus.size(); ++i) {
                int menuLen = static_cast<int>(m_topMenus[i].length()) + 4;
                if (mx >= offset && mx < offset + menuLen) {
                    int sel = static_cast<int>(i);
                    TuiMenu::runMenuLoop(m_documents, m_activeDocIdx, sel, m_topMenus, m_subMenus,
                        [this](int m, int it) { executeMenuAction(m, it); });
                    return;
                }
                offset += menuLen;
            }
            return;
        }

        // Row 1: Document Tabs (Double click = New Tab, Right click = Save/Close dialog)
        if (my == 1) {
            int tabOffset = 1;
            int clickedTab = -1;
            for (size_t i = 0; i < m_documents.size(); ++i) {
                int tabLen = static_cast<int>(m_documents[i].title.length()) + 8;
                if (mx >= tabOffset && mx < tabOffset + tabLen) {
                    clickedTab = static_cast<int>(i);
                    break;
                }
                tabOffset += tabLen;
            }

            // Right click: Offer Save & Close Context Modal
            if ((event.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) && clickedTab >= 0) {
                int res = TuiDialogs::showTabContextMenuModal(m_documents[clickedTab].title);
                if (res == 1) { // Save
                    size_t oldActive = m_activeDocIdx;
                    m_activeDocIdx = clickedTab;
                    saveCurrentFile();
                    m_activeDocIdx = oldActive;
                } else if (res == 2) { // Close
                    closeDocument(clickedTab);
                } else if (res == 3) { // Save & Close
                    size_t oldActive = m_activeDocIdx;
                    m_activeDocIdx = clickedTab;
                    saveCurrentFile();
                    m_activeDocIdx = oldActive;
                    closeDocument(clickedTab);
                }
                return;
            }

            // Left click or Double click
            if (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClickTime).count();

                if (clickedTab >= 0) {
                    if (clickedTab == m_lastClickTabIdx && elapsed < 350) {
                        // Double click on tab: Spawn new tab!
                        EditorDocument d;
                        if (m_defaultCodeMode) d.mode = TuiMode::Code;
                        m_documents.push_back(d);
                        m_activeDocIdx = m_documents.size() - 1;
                        m_lastClickTabIdx = -1;
                        return;
                    }
                    m_activeDocIdx = clickedTab;
                    m_lastClickTabIdx = clickedTab;
                    m_lastClickTime = now;
                    return;
                } else {
                    // Double click on empty space of tab bar: also spawn new tab
                    if (m_lastClickTabIdx == -2 && elapsed < 350) {
                        EditorDocument d;
                        if (m_defaultCodeMode) d.mode = TuiMode::Code;
                        m_documents.push_back(d);
                        m_activeDocIdx = m_documents.size() - 1;
                        m_lastClickTabIdx = -1;
                        return;
                    }
                    m_lastClickTabIdx = -2;
                    m_lastClickTime = now;
                }
                return;
            }
        }

        // Row 2: Toolbar Buttons (No "New" button)
        if (my == 2 && (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
            int toolOffset = 1;
            if (doc().mode == TuiMode::RichText) {
                // {"Save", "PDF", "Open", "Font", "Size", "Bold", "Italic"}
                for (size_t b = 0; b < TuiView::toolbarButtons.size(); ++b) {
                    int btnLen = static_cast<int>(TuiView::toolbarButtons[b].length()) + 4;
                    if (mx >= toolOffset && mx < toolOffset + btnLen) {
                        if (b == 0) saveCurrentFile();
                        else if (b == 1) exportDocument("PDF", "pdf");
                        else if (b == 2) promptOpen();
                        else if (b == 3) TuiActions::handleFontSelection(doc());
                        else if (b == 4) TuiActions::handleFontSizeSelection(doc());
                        else if (b == 5) TuiActions::injectTagAroundSelection(doc(), "<b>", "</b>");
                        else if (b == 6) TuiActions::injectTagAroundSelection(doc(), "<i>", "</i>");
                        return;
                    }
                    toolOffset += btnLen + 1;
                }
            } else {
                // {"Save", "Open", "Comment //", "Duplicate", "Indent", "Unindent"}
                for (size_t b = 0; b < TuiView::codeButtons.size(); ++b) {
                    int btnLen = static_cast<int>(TuiView::codeButtons[b].length()) + 4;
                    if (mx >= toolOffset && mx < toolOffset + btnLen) {
                        if (b == 0) saveCurrentFile();
                        else if (b == 1) promptOpen();
                        else if (b == 2) TuiActions::handleToggleComment(doc());
                        else if (b == 3) TuiActions::handleDuplicate(doc());
                        else if (b == 4) TuiActions::handleIndent(doc(), true);
                        else if (b == 5) TuiActions::handleIndent(doc(), false);
                        return;
                    }
                    toolOffset += btnLen + 1;
                }
            }
            return;
        }

        // Content Lines & Line Number Area Click
        int contentTop = 3;
        int contentBottom = LINES - 2;
        if (my >= contentTop && my <= contentBottom) {
            int lineNumWidth = (doc().mode == TuiMode::Code) ? 6 : 0;
            int targetLine = doc().scrollLine + (my - contentTop);

            // Gutter selection
            if (doc().mode == TuiMode::Code && mx < lineNumWidth && targetLine < static_cast<int>(doc().lines.size())) {
                doc().selectionAnchor = {targetLine, 0};
                doc().cursor.line = targetLine;
                doc().cursor.col = static_cast<int>(doc().lines[targetLine].length());
                doc().hasSelection = true;
                return;
            }

            int targetCol = std::max(0, mx - lineNumWidth);

            if (targetLine < static_cast<int>(doc().lines.size())) {
                targetCol = std::min(targetCol, static_cast<int>(doc().lines[targetLine].length()));
                if (event.bstate & BUTTON1_PRESSED) {
                    doc().cursor.line = targetLine;
                    doc().cursor.col = targetCol;
                    doc().selectionAnchor = doc().cursor;
                    doc().hasSelection = false;
                    m_isDragging = true;
                } else if ((event.bstate & REPORT_MOUSE_POSITION) && m_isDragging) {
                    doc().cursor.line = targetLine;
                    doc().cursor.col = targetCol;
                    if (!(doc().cursor == doc().selectionAnchor)) doc().hasSelection = true;
                } else if (event.bstate & BUTTON1_RELEASED) {
                    if (m_isDragging) {
                        doc().cursor.line = targetLine;
                        doc().cursor.col = targetCol;
                        if (!(doc().cursor == doc().selectionAnchor)) doc().hasSelection = true;
                    }
                    m_isDragging = false;
                }
            }
            return;
        }

        // Mouse Wheel Scroll
        if (event.bstate & 0x10000) {
            if (doc().scrollLine > 0) doc().scrollLine--;
            return;
        }
        if (event.bstate & 0x200000) {
            if (doc().scrollLine + 1 < static_cast<int>(doc().lines.size())) doc().scrollLine++;
            return;
        }
    }

    void handleInput(int ch) {
        if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK) handleMouseEvent(ev);
            return;
        }

        // F1 to F12
        if (ch == KEY_F(1)) { TuiDialogs::showAboutModal(); return; }
        if (ch == KEY_F(2)) {
            EditorDocument d;
            if (m_defaultCodeMode) d.mode = TuiMode::Code;
            m_documents.push_back(d);
            m_activeDocIdx = m_documents.size() - 1;
            return;
        }
        if (ch == KEY_F(3)) {
            if (m_documents.size() > 1) {
                m_activeDocIdx = (m_activeDocIdx + 1) % m_documents.size();
            }
            return;
        }
        if (ch == KEY_F(4)) { TuiActions::handleFindReplace(doc()); return; }
        if (ch == KEY_F(5)) { doc().mode = TuiMode::RichText; return; }
        if (ch == KEY_F(6)) { doc().mode = TuiMode::Code; return; }
        if (ch == KEY_F(7)) { promptOpen(); return; }
        if (ch == KEY_F(8) || ch == 19) { // F8 or Ctrl + S to save
            saveCurrentFile();
            return;
        }
        if (ch == KEY_F(9)) { exportDocument("PDF", "pdf"); return; }
        if (ch == KEY_F(10)) { exportDocument("ODF", "odt"); return; }
        if (ch == KEY_F(11)) {
            int sel = 0;
            TuiMenu::runMenuLoop(m_documents, m_activeDocIdx, sel, m_topMenus, m_subMenus,
                [this](int m, int it) { executeMenuAction(m, it); });
            return;
        }
        if (ch == KEY_F(12) || ch == 17) { promptQuit(); return; }

        // Control shortcuts
        if (ch == 3) { TuiActions::handleCutCopy(doc(), false); return; }
        if (ch == 24) { TuiActions::handleCutCopy(doc(), true); return; }
        if (ch == 4) { doc().hasSelection = false; cutEntireLine(); return; }
        if (ch == 22) { TuiActions::handlePaste(doc()); return; }
        if (ch == 1) {
            doc().selectionAnchor = {0, 0};
            doc().cursor.line = static_cast<int>(doc().lines.size()) - 1;
            doc().cursor.col = static_cast<int>(doc().lines.back().length());
            doc().hasSelection = true;
            return;
        }
        if (ch == 18) { TuiActions::handleRedo(doc()); return; }
        if (ch == 5) { TuiActions::handleUndo(doc()); return; }

        // Tab key: Indent selection or insert 4 spaces
        if (ch == 9) {
            if (doc().hasSelection) {
                TuiActions::handleIndent(doc(), true);
            } else {
                doc().pushUndo();
                doc().lines[doc().cursor.line].insert(doc().cursor.col, "    ");
                doc().cursor.col += 4;
                doc().modified = true;
            }
            return;
        }

        // Shift+Tab / Backtab: Unindent
        if (ch == KEY_BTAB) {
            TuiActions::handleIndent(doc(), false);
            return;
        }

        // Standard navigation
        if (ch == KEY_UP) {
            if (doc().cursor.line > 0) doc().cursor.line--;
            doc().cursor.col = std::min(doc().cursor.col, static_cast<int>(doc().lines[doc().cursor.line].length()));
        } else if (ch == KEY_DOWN) {
            if (doc().cursor.line + 1 < static_cast<int>(doc().lines.size())) doc().cursor.line++;
            doc().cursor.col = std::min(doc().cursor.col, static_cast<int>(doc().lines[doc().cursor.line].length()));
        } else if (ch == KEY_LEFT) {
            if (doc().cursor.col > 0) doc().cursor.col--;
            else if (doc().cursor.line > 0) {
                doc().cursor.line--;
                doc().cursor.col = static_cast<int>(doc().lines[doc().cursor.line].length());
            }
        } else if (ch == KEY_RIGHT) {
            if (doc().cursor.col < static_cast<int>(doc().lines[doc().cursor.line].length())) doc().cursor.col++;
            else if (doc().cursor.line + 1 < static_cast<int>(doc().lines.size())) {
                doc().cursor.line++;
                doc().cursor.col = 0;
            }
        } else if (ch == KEY_PPAGE) {
            doc().cursor.line = std::max(0, doc().cursor.line - (LINES - 4));
            doc().cursor.col = std::min(doc().cursor.col, static_cast<int>(doc().lines[doc().cursor.line].length()));
        } else if (ch == KEY_NPAGE) {
            doc().cursor.line = std::min(static_cast<int>(doc().lines.size()) - 1, doc().cursor.line + (LINES - 4));
            doc().cursor.col = std::min(doc().cursor.col, static_cast<int>(doc().lines[doc().cursor.line].length()));
        } else if (ch == KEY_HOME) {
            doc().cursor.col = 0;
        } else if (ch == KEY_END) {
            doc().cursor.col = static_cast<int>(doc().lines[doc().cursor.line].length());
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (doc().mode == TuiMode::Code && TuiActions::handleSmartBackspace(doc())) {
                return;
            }
            doc().pushUndo();
            if (doc().cursor.col > 0) {
                doc().lines[doc().cursor.line].erase(doc().cursor.col - 1, 1);
                doc().cursor.col--;
                doc().modified = true;
            } else if (doc().cursor.line > 0) {
                int prevLen = static_cast<int>(doc().lines[doc().cursor.line - 1].length());
                doc().lines[doc().cursor.line - 1] += doc().lines[doc().cursor.line];
                doc().lines.erase(doc().lines.begin() + doc().cursor.line);
                doc().cursor.line--;
                doc().cursor.col = prevLen;
                doc().modified = true;
            }
        } else if (ch == 10 || ch == KEY_ENTER) {
            if (doc().mode == TuiMode::Code) {
                TuiActions::handleSmartReturn(doc());
            } else {
                doc().pushUndo();
                std::string cur = doc().lines[doc().cursor.line];
                std::string rem = cur.substr(doc().cursor.col);
                doc().lines[doc().cursor.line] = cur.substr(0, doc().cursor.col);
                doc().lines.insert(doc().lines.begin() + doc().cursor.line + 1, rem);
                doc().cursor.line++;
                doc().cursor.col = 0;
                doc().modified = true;
            }
        } else if (ch >= 32 && ch <= 126) {
            doc().lines[doc().cursor.line].insert(doc().cursor.col, 1, static_cast<char>(ch));
            doc().cursor.col++;
            doc().modified = true;
        }
    }
};

int runCursesApp(int argc, char *argv[]) {
    std::vector<std::string> files;
    bool forceCodeMode = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-c") == 0 || std::strcmp(argv[i], "--console") == 0) {
            continue;
        }
        if (std::strcmp(argv[i], "-C") == 0 || std::strcmp(argv[i], "--code") == 0) {
            forceCodeMode = true;
            continue;
        }
        files.push_back(argv[i]);
    }

    NcursesEditor app;
    return app.run(files, forceCodeMode);
}
