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

namespace {

static std::vector<std::string> wrapDisplaySegments(const std::string &line, int wrapWidth) {
    std::vector<std::string> wrapped;
    if (wrapWidth <= 0 || line.empty()) {
        wrapped.push_back(line);
        return wrapped;
    }

    size_t start = 0;
    while (start < line.size()) {
        size_t end = std::min(line.size(), start + static_cast<size_t>(wrapWidth));
        if (end >= line.size()) {
            wrapped.push_back(line.substr(start));
            break;
        }

        size_t split = end;
        while (split > start && !std::isspace(static_cast<unsigned char>(line[split - 1]))) {
            --split;
        }
        if (split == start) {
            split = end;
        }

        wrapped.push_back(line.substr(start, split - start));
        start = split;
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
            ++start;
        }
    }

    if (wrapped.empty()) {
        wrapped.push_back(line);
    }
    return wrapped;
}

static int effectiveWrapWidth(const EditorDocument &doc, int lineNumWidth) {
    if (doc.wrapMode == TuiWrapMode::Off) {
        return 0;
    }

    const int usable = std::max(20, COLS - lineNumWidth - 2);
    if (doc.wrapWidth > 0) {
        return std::min(doc.wrapWidth, usable);
    }
    return usable;
}

static std::vector<int> buildWrapStarts(const std::vector<std::string> &lines, int wrapWidth) {
    std::vector<int> starts;
    starts.reserve(lines.size() + 1);
    starts.push_back(0);

    int current = 0;
    for (const std::string &line : lines) {
        const auto segments = wrapDisplaySegments(line, wrapWidth);
        current += static_cast<int>(segments.size());
        starts.push_back(current);
    }

    return starts;
}

static int wrappedRowForCursor(const std::string &line, int cursorCol, int wrapWidth) {
    if (wrapWidth <= 0 || cursorCol <= 0) {
        return 0;
    }
    return cursorCol / wrapWidth;
}

} // namespace

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
        if (i + 1 < line.length() && line[i] == '/' && line[i + 1] == '/') {
            attron(COLOR_PAIR(4) | A_DIM);
            mvprintw(screenY, startX + static_cast<int>(i), "%s", line.substr(i).c_str());
            attroff(COLOR_PAIR(4) | A_DIM);
            break;
        }
        if (line[i] == '#') {
            attron(COLOR_PAIR(4) | A_DIM);
            mvprintw(screenY, startX + static_cast<int>(i), "%s", line.substr(i).c_str());
            attroff(COLOR_PAIR(4) | A_DIM);
            break;
        }

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

    move(1, 0);
    clrtoeol();
    int tabOffset = 1;
    for (size_t i = 0; i < documents.size(); ++i) {
        bool active = (i == activeDocIdx);
        if (active) attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(1, tabOffset, "[ %zu: %s%s ]", i + 1, documents[i].title.c_str(), documents[i].modified ? " *" : "");
        if (active) attroff(COLOR_PAIR(3) | A_BOLD);
        tabOffset += static_cast<int>(documents[i].title.length()) + 8;
    }

    int contentTop = 2;
    move(contentTop, 0);
    clrtoeol();

    const auto &buttons = (doc.mode == TuiMode::RichText) ? toolbarButtons : codeButtons;
    int toolOffset = 1;
    for (size_t b = 0; b < buttons.size(); ++b) {
        bool isSave = (buttons[b] == "Save");
        int pair = (isSave && saveFlash) ? 3 : 2;
        attron(COLOR_PAIR(pair) | (saveFlash && isSave ? A_BOLD | A_REVERSE : 0));
        mvprintw(contentTop, toolOffset, "[ %s ]", buttons[b].c_str());
        attroff(COLOR_PAIR(pair) | (saveFlash && isSave ? A_BOLD | A_REVERSE : 0));
        toolOffset += static_cast<int>(buttons[b].length()) + 5;
    }
    contentTop++;

    int contentHeight = LINES - contentTop - 1;
    int lineNumWidth = (doc.mode == TuiMode::Code) ? 6 : 0;
    int wrapWidth = effectiveWrapWidth(doc, lineNumWidth);
    bool useWrap = (doc.wrapMode != TuiWrapMode::Off && wrapWidth > 0);

    const char *wrapLabel = (doc.wrapMode == TuiWrapMode::Off) ? "Off" :
                            (doc.wrapMode == TuiWrapMode::Soft ? "Soft" : "Hard");

    int runningDepth = 0;
    if (doc.mode == TuiMode::Code) {
        for (int l = 0; l < doc.scrollLine && l < static_cast<int>(doc.lines.size()); ++l) {
            for (char c : doc.lines[l]) {
                if (c == '(' || c == '{' || c == '[') runningDepth++;
                else if (c == ')' || c == '}' || c == ']') runningDepth = std::max(0, runningDepth - 1);
            }
        }
    }

    std::vector<int> wrapStarts;
    int scrollBase = doc.scrollLine;
    if (useWrap) {
        wrapStarts = buildWrapStarts(doc.lines, wrapWidth);
        if (!wrapStarts.empty() && static_cast<size_t>(doc.scrollLine) < wrapStarts.size()) {
            scrollBase = wrapStarts[std::min(doc.scrollLine, static_cast<int>(wrapStarts.size() - 1))];
        }
    }

    for (int y = 0; y < contentHeight; ++y) {
        int screenY = contentTop + y;
        move(screenY, 0);
        clrtoeol();

        int rowIndex = scrollBase + y;
        int logicalLine = doc.scrollLine;
        std::string displayText;

        if (useWrap) {
            if (!wrapStarts.empty()) {
                int displayRow = rowIndex;
                while (logicalLine + 1 < static_cast<int>(doc.lines.size()) && displayRow >= wrapStarts[logicalLine + 1]) {
                    ++logicalLine;
                }
                if (logicalLine >= static_cast<int>(doc.lines.size())) {
                    continue;
                }

                const auto segments = wrapDisplaySegments(doc.lines[logicalLine], wrapWidth);
                int segmentIndex = displayRow - wrapStarts[logicalLine];
                if (segmentIndex >= 0 && segmentIndex < static_cast<int>(segments.size())) {
                    displayText = segments[segmentIndex];
                } else {
                    continue;
                }
            }
        } else if (logicalLine < static_cast<int>(doc.lines.size())) {
            logicalLine = doc.scrollLine + y;
            if (logicalLine < static_cast<int>(doc.lines.size())) {
                displayText = doc.lines[logicalLine];
            }
        }

        if (logicalLine >= static_cast<int>(doc.lines.size())) {
            continue;
        }

        if (doc.mode == TuiMode::Code && !useWrap) {
            if (doc.scrollLine + y < static_cast<int>(doc.lines.size())) {
                attron(COLOR_PAIR(3));
                mvprintw(screenY, 0, "%4d |", doc.scrollLine + y + 1);
                attroff(COLOR_PAIR(3));
            }
        }

        if (doc.mode == TuiMode::Code && useWrap) {
            if (logicalLine < static_cast<int>(doc.lines.size())) {
                attron(COLOR_PAIR(3));
                mvprintw(screenY, 0, "%4d |", logicalLine + 1);
                attroff(COLOR_PAIR(3));
            }
        }

        if (displayText.empty()) {
            continue;
        }

        if (doc.mode == TuiMode::Code) {
            highlightCodeLine(screenY, lineNumWidth, displayText, runningDepth);
            for (char c : displayText) {
                if (c == '(' || c == '{' || c == '[') runningDepth++;
                else if (c == ')' || c == '}' || c == ']') runningDepth = std::max(0, runningDepth - 1);
            }
        } else {
            for (int x = 0; x < static_cast<int>(displayText.length()) && x + lineNumWidth < COLS; ++x) {
                mvaddch(screenY, lineNumWidth + x, displayText[x]);
            }
        }
    }

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

    mvprintw(LINES - 1, 1,
             "Mode: %s | L:%d C:%d | W:%zu C:%zu | %s | %s | Wrap:%s | F1:Help F8/Ctrl+S:Save F11:Menu F12:Quit",
             doc.mode == TuiMode::RichText ? "Rich" : "Code",
             doc.cursor.line + 1, doc.cursor.col + 1, words, chars,
             doc.encoding.c_str(), doc.eolFormat.c_str(), wrapLabel);
    attroff(COLOR_PAIR(2));

    int screenCursorY = contentTop + (doc.cursor.line - doc.scrollLine);
    if (useWrap) {
        int visualRow = wrapStarts.empty() ? 0 : wrapStarts[std::min(doc.cursor.line, static_cast<int>(wrapStarts.size() - 1))];
        visualRow += wrappedRowForCursor(doc.lines[std::min(doc.cursor.line, static_cast<int>(doc.lines.size() - 1))], doc.cursor.col, wrapWidth);
        screenCursorY = contentTop + (visualRow - scrollBase);
    }

    if (screenCursorY >= contentTop && screenCursorY < contentTop + contentHeight) {
        curs_set(1);
        move(screenCursorY, lineNumWidth + std::min(doc.cursor.col, static_cast<int>(COLS - lineNumWidth - 1)));
    } else {
        curs_set(0);
    }
    refresh();
}

} // namespace TuiView
