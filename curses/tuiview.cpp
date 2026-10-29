#include "tuiview.h"
#include <curses.h>
#include <sstream>
#include <algorithm>
#include <regex>
#include <unordered_set>
#include <vector>

namespace TuiView {

const std::vector<std::string> toolbarButtons = {
    "Save", "Open", "Font", "Size", "Bold", "Italic"
};

const std::vector<std::string> codeButtons = {
    "Save", "Open", "Comment //", "Duplicate", "Indent", "Unindent"
};

static void highlightCodeLine(int screenY, int startX, const std::string &line, int initialBracketDepth) {
    static const std::unordered_set<std::string> keywords = {
        "char", "class", "const", "double", "enum", "explicit", "friend", "inline",
        "int", "long", "namespace", "operator", "private", "protected", "public",
        "short", "signals", "signed", "slots", "static", "struct", "template",
        "typedef", "typename", "union", "unsigned", "virtual", "void", "volatile",
        "bool", "auto", "return", "if", "else", "for", "while", "do", "switch",
        "case", "default", "break", "continue", "def", "import", "from", "function",
        "let", "var", "true", "false", "nullptr", "NULL"
    };

    static const int bracketColorPairs[5] = {3, 4, 1, 2, 6};

    int depth = initialBracketDepth;
    size_t i = 0;

    while (i < line.length() && startX + static_cast<int>(i) < COLS) {
        // Comment: //
        if (i + 1 < line.length() && line[i] == '/' && line[i + 1] == '/') {
            attron(COLOR_PAIR(4) | A_DIM);
            mvprintw(screenY, startX + static_cast<int>(i), "%s", line.substr(i).c_str());
            attroff(COLOR_PAIR(4) | A_DIM);
            break;
        }
        // Comment: #
        if (line[i] == '#') {
            attron(COLOR_PAIR(4) | A_DIM);
            mvprintw(screenY, startX + static_cast<int>(i), "%s", line.substr(i).c_str());
            attroff(COLOR_PAIR(4) | A_DIM);
            break;
        }

        // Strings
        if (line[i] == '"' || line[i] == '\'') {
            char q = line[i];
            size_t j = i + 1;
            while (j < line.length()) {
                if (line[j] == q && line[j - 1] != '\\') {
                    j++;
                    break;
                }
                j++;
            }
            attron(COLOR_PAIR(3));
            for (size_t k = i; k < j && startX + static_cast<int>(k) < COLS; ++k) {
                mvaddch(screenY, startX + static_cast<int>(k), line[k]);
            }
            attroff(COLOR_PAIR(3));
            i = j;
            continue;
        }

        // Rainbow bracket pairs
        if (line[i] == '(' || line[i] == '{' || line[i] == '[') {
            int pairIdx = bracketColorPairs[depth % 5];
            attron(COLOR_PAIR(pairIdx) | A_BOLD);
            mvaddch(screenY, startX + static_cast<int>(i), line[i]);
            attroff(COLOR_PAIR(pairIdx) | A_BOLD);
            depth++;
            i++;
            continue;
        }
        if (line[i] == ')' || line[i] == '}' || line[i] == ']') {
            depth = std::max(0, depth - 1);
            int pairIdx = bracketColorPairs[depth % 5];
            attron(COLOR_PAIR(pairIdx) | A_BOLD);
            mvaddch(screenY, startX + static_cast<int>(i), line[i]);
            attroff(COLOR_PAIR(pairIdx) | A_BOLD);
            i++;
            continue;
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(line[i])) || (line[i] == '-' && i + 1 < line.length() && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            size_t j = i + 1;
            while (j < line.length() && (std::isalnum(static_cast<unsigned char>(line[j])) || line[j] == '.')) {
                j++;
            }
            attron(COLOR_PAIR(2));
            for (size_t k = i; k < j && startX + static_cast<int>(k) < COLS; ++k) {
                mvaddch(screenY, startX + static_cast<int>(k), line[k]);
            }
            attroff(COLOR_PAIR(2));
            i = j;
            continue;
        }

        // Identifiers / Keywords
        if (std::isalpha(static_cast<unsigned char>(line[i])) || line[i] == '_') {
            size_t j = i + 1;
            while (j < line.length() && (std::isalnum(static_cast<unsigned char>(line[j])) || line[j] == '_')) {
                j++;
            }
            std::string word = line.substr(i, j - i);
            if (keywords.count(word)) {
                attron(COLOR_PAIR(1) | A_BOLD);
                mvprintw(screenY, startX + static_cast<int>(i), "%s", word.c_str());
                attroff(COLOR_PAIR(1) | A_BOLD);
            } else {
                for (size_t k = i; k < j && startX + static_cast<int>(k) < COLS; ++k) {
                    mvaddch(screenY, startX + static_cast<int>(k), line[k]);
                }
            }
            i = j;
            continue;
        }

        mvaddch(screenY, startX + static_cast<int>(i), line[i]);
        i++;
    }
}

void render(
    const std::vector<EditorDocument> &documents,
    size_t activeDocIdx,
    bool menuActive,
    int menuSelect,
    const std::vector<std::string> &topMenus,
    bool saveFlash
) {
    if (documents.empty() || activeDocIdx >= documents.size()) return;
    const EditorDocument &doc = documents[activeDocIdx];

    erase();

    // 1. Top Menu Bar (Solid band across full terminal width)
    attron(COLOR_PAIR(1));
    move(0, 0);
    for (int c = 0; c < COLS; ++c) addch(' ');
    int offset = 2;
    for (size_t i = 0; i < topMenus.size(); ++i) {
        if (menuActive && static_cast<int>(i) == menuSelect) attron(A_REVERSE);
        mvprintw(0, offset, " %s ", topMenus[i].c_str());
        if (menuActive && static_cast<int>(i) == menuSelect) wattroff(stdscr, A_REVERSE);
        offset += static_cast<int>(topMenus[i].length()) + 4;
    }
    attroff(COLOR_PAIR(1));

    // 2. Tabs Row
    move(1, 0);
    clrtoeol();
    int tabOffset = 1;
    for (size_t i = 0; i < documents.size(); ++i) {
        bool active = (i == activeDocIdx);
        // Active tab matches line numbers color: COLOR_PAIR(3) (Yellow)
        if (active) attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(1, tabOffset, "[ %zu: %s%s ]", i + 1, documents[i].title.c_str(), documents[i].modified ? " *" : "");
        if (active) attroff(COLOR_PAIR(3) | A_BOLD);
        tabOffset += static_cast<int>(documents[i].title.length()) + 8;
    }

    // 3. Tool Bar Row
    int contentTop = 2;
    move(contentTop, 0);
    clrtoeol();

    const auto &buttons = (doc.mode == TuiMode::RichText) ? toolbarButtons : codeButtons;
    int toolOffset = 1;
    for (size_t b = 0; b < buttons.size(); ++b) {
        bool isSave = (buttons[b] == "Save");
        int pair = (isSave && saveFlash) ? 3 : 2; // Flash Save button yellow (COLOR_PAIR(3))
        attron(COLOR_PAIR(pair) | (saveFlash && isSave ? A_BOLD | A_REVERSE : 0));
        mvprintw(contentTop, toolOffset, "[ %s ]", buttons[b].c_str());
        attroff(COLOR_PAIR(pair) | (saveFlash && isSave ? A_BOLD | A_REVERSE : 0));
        toolOffset += static_cast<int>(buttons[b].length()) + 5;
    }
    contentTop++; // Row 3 is content area

    // 4. Viewport lines
    int contentHeight = LINES - contentTop - 1;
    int scrollLine = doc.scrollLine;
    int lineNumWidth = (doc.mode == TuiMode::Code) ? 6 : 0;

    int runningDepth = 0;
    if (doc.mode == TuiMode::Code) {
        for (int l = 0; l < scrollLine && l < static_cast<int>(doc.lines.size()); ++l) {
            for (char c : doc.lines[l]) {
                if (c == '(' || c == '{' || c == '[') runningDepth++;
                else if (c == ')' || c == '}' || c == ']') runningDepth = std::max(0, runningDepth - 1);
            }
        }
    }

    for (int y = 0; y < contentHeight; ++y) {
        int lineIdx = scrollLine + y;
        int screenY = contentTop + y;
        move(screenY, 0);
        clrtoeol();

        if (lineIdx < static_cast<int>(doc.lines.size())) {
            if (doc.mode == TuiMode::Code) {
                attron(COLOR_PAIR(3));
                mvprintw(screenY, 0, "%4d |", lineIdx + 1);
                attroff(COLOR_PAIR(3));
            }

            const std::string &l = doc.lines[lineIdx];

            if (doc.hasSelection) {
                for (int x = 0; x < static_cast<int>(l.length()) && x + lineNumWidth < COLS; ++x) {
                    TextPosition p{lineIdx, x}, s = doc.selectionAnchor, e = doc.cursor;
                    if (e < s) std::swap(s, e);
                    bool inSel = (!(p < s) && (p < e));
                    if (inSel) attron(COLOR_PAIR(5));
                    mvaddch(screenY, lineNumWidth + x, l[x]);
                    if (inSel) attroff(COLOR_PAIR(5));
                }
            } else {
                if (doc.mode == TuiMode::Code) {
                    highlightCodeLine(screenY, lineNumWidth, l, runningDepth);
                    for (char c : l) {
                        if (c == '(' || c == '{' || c == '[') runningDepth++;
                        else if (c == ')' || c == '}' || c == ']') runningDepth = std::max(0, runningDepth - 1);
                    }
                } else {
                    for (int x = 0; x < static_cast<int>(l.length()) && x + lineNumWidth < COLS; ++x) {
                        mvaddch(screenY, lineNumWidth + x, l[x]);
                    }
                }
            }
        }
    }

    // 5. Status Bar Row (Solid band across full terminal width)
    attron(COLOR_PAIR(2));
    move(LINES - 1, 0);
    for (int c = 0; c < COLS; ++c) addch(' ');

    size_t words = 0, chars = 0;
    for (const auto &l : doc.lines) {
        chars += l.length();
        std::stringstream ss(l);
        std::string w;
        while (ss >> w) words++;
    }

    mvprintw(LINES - 1, 1, "Mode: %s | L:%d C:%d | W:%zu C:%zu | %s | %s | F1:Help F8/Ctrl+S:Save F11:Menu F12:Quit",
             doc.mode == TuiMode::RichText ? "Rich" : "Code",
             doc.cursor.line + 1, doc.cursor.col + 1, words, chars,
             doc.encoding.c_str(), doc.eolFormat.c_str());
    attroff(COLOR_PAIR(2));

    // Place physical terminal cursor if within visible viewport
    int screenCursorY = contentTop + (doc.cursor.line - scrollLine);
    if (screenCursorY >= contentTop && screenCursorY < contentTop + contentHeight) {
        curs_set(1);
        move(screenCursorY, lineNumWidth + doc.cursor.col);
    } else {
        curs_set(0);
    }
    refresh();
}

} // namespace TuiView
