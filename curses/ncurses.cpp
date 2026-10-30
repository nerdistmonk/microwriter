#include "ncurses.h"
#include "tuitypes.h"
#include "tuiconverter.h"
#include "tuiinput.h"
#include "tuidialogs.h"
#include "tuiactions.h"
#include "tuiview.h"
#include "tuimenu.h"

#include <curses.h>
#include <clocale>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <regex>

static const std::vector<std::string> g_tuiStrictCodeExts = {
    "cpp", "c", "h", "hpp", "cc", "cxx", "py", "js", "ts", "json", "css",
    "xml", "sh", "bash", "ini", "cfg", "conf", "yaml", "yml", "cmake"
};

class NcursesEditor {
public:
    NcursesEditor() = default;

    int run(const std::vector<std::string> &initialFiles, bool forceCodeMode = false) {
        m_defaultCodeMode = forceCodeMode;

        std::setlocale(LC_ALL, "");

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

        TuiEditorContext ctx{
            m_documents,
            m_activeDocIdx,
            m_running,
            m_isDragging,
            m_saveFlashActive,
            m_defaultCodeMode,
            m_lastClickTabIdx,
            m_lastClickTime,
            m_topMenus,
            m_subMenus,
            [this]() { saveCurrentFile(); },
            [this]() { saveAsFile(); },
            [this]() { promptOpen(); },
            [this](size_t idx) { closeDocument(idx); },
            [this]() { promptQuit(); },
            [this](TuiMode mode) { switchDocumentMode(mode); },
            [this](const std::string &lbl, const std::string &ext) { exportDocument(lbl, ext); },
            [this](int m, int it) { executeMenuAction(m, it); }
        };

        while (m_running) {
            updateScroll();
            TuiView::render(m_documents, m_activeDocIdx, false, 0, m_topMenus, m_saveFlashActive);
            if (m_saveFlashActive) {
                m_saveFlashActive = false;
            }
            int ch = getch();
            TuiInput::handleInput(ch, ctx);
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

    int m_lastClickTabIdx = -1;
    std::chrono::steady_clock::time_point m_lastClickTime;

    const std::vector<std::string> m_topMenus = {"File", "Edit", "Format", "Tools", "Help"};
    const std::vector<std::vector<std::string>> m_subMenus = {
        {
            "New", "Open", "Save", "Save As", "Switch Mode",
            "Encoding: UTF-8", "Encoding: ANSI (Windows-1252)", "Encoding: ISO-8859-1",
            "EOL: Unix (LF)", "EOL: Windows (CRLF)", "EOL: Mac (CR)",
            "Wrap: Off", "Wrap: Soft", "Wrap: Hard", "Exit"
        },
        {"Undo", "Redo", "Cut", "Copy", "Paste", "Duplicate Line/Block", "Find & Replace"},
        {"Select Font", "Font Size", "Text Color", "Bold", "Italic", "Underline", "Bullet List", "Numbered List", "Blockquote", "Insert Table", "Horizontal Line"},
        {"Spell Check (Aspell)", "Toggle Comment //", "Move Line Up", "Move Line Down", "Indent", "Unindent", "Strip Blank Lines", "Sort Lines", "Reverse Lines", "To Uppercase", "To Lowercase"},
        {"About MicroWriter TUI", "Regex Cheat Sheet", "HTML Quick Reference", "Sed & Utilities Cheat Sheet", "Useful Bash Scripts"}
    };

    EditorDocument& doc() { return m_documents[m_activeDocIdx]; }

    void updateScroll() {
        int contentTop = 3;
        int contentHeight = std::max(1, LINES - contentTop - 1);
        if (doc().cursor.line < doc().scrollLine) {
            doc().scrollLine = doc().cursor.line;
        } else if (doc().cursor.line >= doc().scrollLine + contentHeight) {
            doc().scrollLine = doc().cursor.line - contentHeight + 1;
        }
    }

    bool isDocStrictCode(const EditorDocument &d) const {
        std::string ext = "";
        size_t dot = d.filePath.find_last_of('.');
        if (dot != std::string::npos) {
            ext = d.filePath.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }
        return std::find(g_tuiStrictCodeExts.begin(), g_tuiStrictCodeExts.end(), ext) != g_tuiStrictCodeExts.end();
    }

    void applyWrapMode(TuiWrapMode mode) {
        doc().wrapMode = mode;
        if (mode == TuiWrapMode::Hard) {
            std::vector<std::string> wrapped;
            const int width = doc().wrapWidth > 0 ? std::min(doc().wrapWidth, std::max(20, COLS - 10)) : std::max(20, COLS - 10);
            for (const auto &line : doc().lines) {
                if (line.empty()) {
                    wrapped.push_back("");
                    continue;
                }

                size_t start = 0;
                while (start < line.size()) {
                    size_t end = std::min(line.size(), start + static_cast<size_t>(width));
                    if (end >= line.size()) {
                        wrapped.push_back(line.substr(start));
                        break;
                    }
                    size_t split = end;
                    while (split > start && !std::isspace(static_cast<unsigned char>(line[split - 1]))) {
                        --split;
                    }
                    if (split == start) split = end;
                    wrapped.push_back(line.substr(start, split - start));
                    start = split;
                    while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
                        ++start;
                    }
                }
            }
            doc().lines = wrapped.empty() ? std::vector<std::string>{""} : wrapped;
            doc().modified = true;
        }
    }

    void switchDocumentMode(TuiMode targetMode) {
        if (doc().mode == targetMode) return;

        if (targetMode == TuiMode::RichText && isDocStrictCode(doc())) {
            return;
        }

        bool prevModified = doc().modified;

        std::string ext = "";
        size_t dot = doc().filePath.find_last_of('.');
        if (dot != std::string::npos) {
            ext = doc().filePath.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        if (targetMode == TuiMode::Code) {
            std::string raw = TuiConverter::serializeDocToRaw(doc());
            doc().lines.clear();
            std::stringstream ss(raw);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                doc().lines.push_back(line);
            }
            if (doc().lines.empty()) doc().lines.push_back("");
            doc().mode = TuiMode::Code;
        } else {
            std::ostringstream ss;
            for (size_t i = 0; i < doc().lines.size(); ++i) {
                ss << doc().lines[i];
                if (i + 1 < doc().lines.size()) ss << "\n";
            }
            std::string rawBuffer = ss.str();
            TuiConverter::deserializeRawToRich(doc(), rawBuffer);
            doc().mode = TuiMode::RichText;
        }

        doc().cursor.line = std::min(doc().cursor.line, static_cast<int>(doc().lines.size()) - 1);
        doc().cursor.col = std::min(doc().cursor.col, static_cast<int>(doc().lines[doc().cursor.line].length()));
        doc().scrollLine = 0;
        doc().hasSelection = false;
        doc().modified = prevModified;
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
            "cpp", "c", "h", "hpp", "cc", "cxx", "py", "js", "ts", "json", "css",
            "xml", "sh", "bash", "ini", "cfg", "conf", "yaml", "yml", "cmake", "txt"
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

            if (d.mode == TuiMode::RichText) {
                TuiConverter::deserializeRawToRich(d, rawContent);
            } else {
                std::stringstream ss(rawContent);
                std::string line;
                while (std::getline(ss, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    d.lines.push_back(line);
                }
                if (d.lines.empty()) d.lines.push_back("");
            }
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

    void saveAsFile() {
        std::string curPath = doc().filePath;
        if (curPath.empty()) curPath = "untitled";

        int fmtType = 0;
        if (curPath.length() >= 3 && curPath.compare(curPath.length() - 3, 3, ".md") == 0) {
            fmtType = 1;
        } else if (curPath.length() >= 4 && curPath.compare(curPath.length() - 4, 4, ".txt") == 0) {
            fmtType = 2;
        }

        if (!TuiDialogs::showSaveAsModal(curPath, fmtType)) {
            return;
        }

        doc().filePath = curPath;
        size_t lastSlash = curPath.find_last_of("/\\");
        doc().title = (lastSlash == std::string::npos) ? curPath : curPath.substr(lastSlash + 1);

        writeDocumentToFile(curPath, fmtType);
    }

    void saveCurrentFile() {
        if (doc().filePath.empty()) {
            saveAsFile();
            return;
        }
        writeDocumentToFile(doc().filePath, -1);
    }

    void writeDocumentToFile(const std::string &path, int targetFormat = -1) {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) return;

        std::string ext = "";
        size_t dot = path.find_last_of('.');
        if (dot != std::string::npos) {
            ext = path.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        bool savingAsHtml = (ext == "html" || ext == "htm" || targetFormat == 0);
        bool savingAsMd = (ext == "md" || targetFormat == 1);
        bool savingAsTxt = (ext == "txt" || targetFormat == 2);

        std::string finalOutput;

        if (savingAsHtml) {
            bool bufferHasHtmlDoc = false;
            for (const auto &l : doc().lines) {
                if (l.find("<!DOCTYPE") != std::string::npos || l.find("<html") != std::string::npos) {
                    bufferHasHtmlDoc = true;
                    break;
                }
            }

            if (doc().mode == TuiMode::Code && bufferHasHtmlDoc) {
                std::ostringstream ss;
                for (size_t i = 0; i < doc().lines.size(); ++i) {
                    ss << doc().lines[i];
                    if (i + 1 < doc().lines.size()) ss << "\n";
                }
                finalOutput = ss.str();
            } else {
                finalOutput = TuiConverter::serializeLinesToHtml(doc().lines);
            }
        } else if (savingAsMd) {
            bool bufferHasHtml = false;
            for (const auto &l : doc().lines) {
                if (l.find("<p") != std::string::npos || l.find("<table") != std::string::npos ||
                    l.find("<h") != std::string::npos || l.find("<strong") != std::string::npos ||
                    l.find("<b>") != std::string::npos || l.find("<em") != std::string::npos ||
                    l.find("<i>") != std::string::npos || l.find("<code") != std::string::npos ||
                    l.find("<hr") != std::string::npos || l.find("• ") != std::string::npos) {
                    bufferHasHtml = true;
                    break;
                }
            }

            if (doc().mode == TuiMode::Code && !bufferHasHtml) {
                std::ostringstream ss;
                for (size_t i = 0; i < doc().lines.size(); ++i) {
                    ss << doc().lines[i];
                    if (i + 1 < doc().lines.size()) ss << "\n";
                }
                finalOutput = ss.str();
            } else {
                finalOutput = TuiConverter::serializeLinesToMarkdown(doc().lines);
            }
        } else if (savingAsTxt) {
            finalOutput = TuiConverter::serializeLinesToPlainText(doc().lines);
        } else {
            std::ostringstream ss;
            for (size_t i = 0; i < doc().lines.size(); ++i) {
                ss << doc().lines[i];
                if (i + 1 < doc().lines.size()) ss << "\n";
            }
            finalOutput = ss.str();
        }

        std::string eolStr = (doc().eolFormat == "CRLF") ? "\r\n" : ((doc().eolFormat == "CR") ? "\r" : "\n");
        if (eolStr != "\n") {
            finalOutput = std::regex_replace(finalOutput, std::regex("\r\n|\r|\n"), eolStr);
        }

        out << finalOutput;
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
        std::string p = doc().filePath;
        int filterType = 0;
        if (TuiDialogs::showOpenFileModal(p, filterType)) {
            if (!p.empty()) {
                loadFile(p);
                m_activeDocIdx = m_documents.size() - 1;
            }
        }
    }

    void closeDocument(size_t index) {
        if (index >= m_documents.size()) return;
        if (m_documents[index].modified) {
            int res = TuiDialogs::showConfirmModal("Save Changes?", "Unsaved changes in " + m_documents[index].title + ". Save first?");
            if (res == 1) {
                size_t oldActive = m_activeDocIdx;
                m_activeDocIdx = index;
                saveCurrentFile();
                m_activeDocIdx = oldActive;
            } else if (res == 0) {
                return;
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
        int res = TuiDialogs::showConfirmModal("Quit MicroWriter", "You have unsaved changes. Save before quitting?");
        if (res == 1) {
            saveCurrentFile();
            m_running = false;
        } else if (res == 2) {
            m_running = false;
        }
    }

    void executeMenuAction(int menuIdx, int itemIdx) {
        if (menuIdx == 0) {
            if (itemIdx == 0) {
                EditorDocument d;
                if (m_defaultCodeMode) d.mode = TuiMode::Code;
                m_documents.push_back(d);
                m_activeDocIdx = m_documents.size() - 1;
            }
            else if (itemIdx == 1) promptOpen();
            else if (itemIdx == 2) saveCurrentFile();
            else if (itemIdx == 3) saveAsFile();
            else if (itemIdx == 4) switchDocumentMode((doc().mode == TuiMode::RichText) ? TuiMode::Code : TuiMode::RichText);
            else if (itemIdx == 5) { doc().encoding = "UTF-8"; doc().modified = true; }
            else if (itemIdx == 6) { doc().encoding = "Windows-1252"; doc().modified = true; }
            else if (itemIdx == 7) { doc().encoding = "ISO-8859-1"; doc().modified = true; }
            else if (itemIdx == 8) { doc().eolFormat = "LF"; doc().modified = true; }
            else if (itemIdx == 9) { doc().eolFormat = "CRLF"; doc().modified = true; }
            else if (itemIdx == 10) { doc().eolFormat = "CR"; doc().modified = true; }
            else if (itemIdx == 11) { applyWrapMode(TuiWrapMode::Off); }
            else if (itemIdx == 12) { applyWrapMode(TuiWrapMode::Soft); }
            else if (itemIdx == 13) { applyWrapMode(TuiWrapMode::Hard); }
            else if (itemIdx == 14) promptQuit();
        } else if (menuIdx == 1) {
            if (itemIdx == 0) TuiActions::handleUndo(doc());
            else if (itemIdx == 1) TuiActions::handleRedo(doc());
            else if (itemIdx == 2) TuiActions::handleCutCopy(doc(), true);
            else if (itemIdx == 3) TuiActions::handleCutCopy(doc(), false);
            else if (itemIdx == 4) TuiActions::handlePaste(doc());
            else if (itemIdx == 5) TuiActions::handleDuplicate(doc());
            else if (itemIdx == 6) TuiActions::handleFindReplace(doc());
        } else if (menuIdx == 2) {
            if (doc().mode != TuiMode::RichText) return;
            if (itemIdx == 0) TuiActions::handleFontSelection(doc());
            else if (itemIdx == 1) TuiActions::handleFontSizeSelection(doc());
            else if (itemIdx == 2) TuiActions::handleColorSelection(doc());
            else if (itemIdx == 3) TuiActions::injectTagAroundSelection(doc(), "<b>", "</b>");
            else if (itemIdx == 4) TuiActions::injectTagAroundSelection(doc(), "<i>", "</i>");
            else if (itemIdx == 5) TuiActions::injectTagAroundSelection(doc(), "<u>", "</u>");
            else if (itemIdx == 6) TuiActions::handleInsertBulletList(doc());
            else if (itemIdx == 7) TuiActions::handleInsertNumberedList(doc());
            else if (itemIdx == 8) TuiActions::injectTagAroundSelection(doc(), "<blockquote>", "</blockquote>");
            else if (itemIdx == 9) TuiActions::handleInsertTable(doc());
            else if (itemIdx == 10) {
                doc().pushUndo();
                doc().lines[doc().cursor.line].insert(doc().cursor.col, "<hr>");
                doc().cursor.col += 4;
                doc().modified = true;
            }
        } else if (menuIdx == 3) {
            if (itemIdx == 0) TuiActions::handleSpellCheck(doc());
            else if (itemIdx == 1) TuiActions::handleToggleComment(doc());
            else if (itemIdx == 2) TuiActions::handleMoveLine(doc(), true);
            else if (itemIdx == 3) TuiActions::handleMoveLine(doc(), false);
            else if (itemIdx == 4) TuiActions::handleIndent(doc(), true);
            else if (itemIdx == 5) TuiActions::handleIndent(doc(), false);
            else if (itemIdx == 6) TuiActions::handleStripBlankLines(doc());
            else if (itemIdx == 7) TuiActions::handleSortLines(doc());
            else if (itemIdx == 8) TuiActions::handleReverseLines(doc());
            else if (itemIdx == 9) TuiActions::handleChangeCase(doc(), true);
            else if (itemIdx == 10) TuiActions::handleChangeCase(doc(), false);
        } else if (menuIdx == 4) {
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