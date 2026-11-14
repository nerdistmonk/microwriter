#include "tuiview.h"
#include "tuiactions.h"
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

struct DisplaySegment {
    std::string text;
    int colOffset = 0;
};

static std::vector<DisplaySegment> wrapDisplaySegmentsWithOffsets(const std::string &line, int wrapWidth) {
    std::vector<DisplaySegment> segments;
    if (wrapWidth <= 0 || line.empty()) {
        segments.push_back({line, 0});
        return segments;
    }

    size_t start = 0;
    while (start < line.size()) {
        size_t end = std::min(line.size(), start + static_cast<size_t>(wrapWidth));
        if (end >= line.size()) {
            segments.push_back({line.substr(start), static_cast<int>(start)});
            break;
        }

        size_t split = end;
        while (split > start && !std::isspace(static_cast<unsigned char>(line[split - 1]))) {
            --split;
        }
        if (split == start) {
            split = end;
        }

        segments.push_back({line.substr(start, split - start), static_cast<int>(start)});
        start = split;
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
            ++start;
        }
    }

    if (segments.empty()) {
        segments.push_back({line, 0});
    }
    return segments;
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
        const auto segments = wrapDisplaySegmentsWithOffsets(line, wrapWidth);
        current += static_cast<int>(segments.size());
        starts.push_back(current);
    }

    return starts;
}

static int wrappedRowForCursor(const std::vector<DisplaySegment> &segments, int cursorCol) {
    if (segments.empty() || cursorCol <= 0) return 0;
    for (int i = static_cast<int>(segments.size()) - 1; i >= 0; --i) {
        if (cursorCol >= segments[i].colOffset) {
            return i;
        }
    }
    return 0;
}

static void printCharWithSelection(int screenY, int screenX, char ch, bool selected, chtype baseAttr = 0) {
    if (screenX >= COLS) return;
    if (baseAttr) attron(baseAttr);
    if (selected) attron(A_REVERSE);
    mvaddch(screenY, screenX, ch);
    if (selected) attroff(A_REVERSE);
    if (baseAttr) attroff(baseAttr);
}

// Classifies a whole logical line, so colours never depend on which part of it
// is on screen. Fills attrs (if given) with one attribute per character and
// returns the bracket depth after the line.
static int scanCodeLine(const std::string &line, int depth, std::vector<chtype> *attrs) {
    if (!attrs && line.find_first_of("([{)]}") == std::string::npos) {
        return depth;
    }

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

    if (attrs) attrs->assign(line.size(), 0);
    auto paint = [&](size_t from, size_t to, chtype attr) {
        if (!attrs) return;
        for (size_t k = from; k < to; ++k) (*attrs)[k] = attr;
    };
    auto isWordChar = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    };

    const size_t lastNonSpace = line.find_last_not_of(" \t");
    size_t i = 0;

    while (i < line.length()) {
        const char c = line[i];
        const bool isTrailing = (lastNonSpace == std::string::npos || i > lastNonSpace) &&
                                (c == ' ' || c == '\t');

        if (isTrailing) {
            paint(i, i + 1, COLOR_PAIR(7));
            i++;
            continue;
        }

        if ((c == '/' && i + 1 < line.length() && line[i + 1] == '/') || c == '#') {
            paint(i, line.length(), COLOR_PAIR(4) | A_DIM);
            break;
        }

        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            bool closed = false;
            while (j < line.length()) {
                if (line[j] == '\\') {
                    j += 2;
                    continue;
                }
                if (line[j] == c) {
                    closed = true;
                    j++;
                    break;
                }
                j++;
            }
            if (closed) {
                paint(i, j, COLOR_PAIR(3));
                i = j;
            } else {
                // A quote with no partner (an apostrophe, say) is just a character.
                i++;
            }
            continue;
        }

        if (c == '(' || c == '{' || c == '[') {
            paint(i, i + 1, COLOR_PAIR(bracketColorPairs[depth % 5]) | A_BOLD);
            depth++;
            i++;
            continue;
        }
        if (c == ')' || c == '}' || c == ']') {
            depth = std::max(0, depth - 1);
            paint(i, i + 1, COLOR_PAIR(bracketColorPairs[depth % 5]) | A_BOLD);
            i++;
            continue;
        }

        // A leading '-' belongs to the number only when it isn't subtracting from a name or bracket.
        const bool signedNumber = c == '-' && i + 1 < line.length() &&
            std::isdigit(static_cast<unsigned char>(line[i + 1])) &&
            (i == 0 || !(isWordChar(line[i - 1]) || line[i - 1] == ')' || line[i - 1] == ']'));

        if (std::isdigit(static_cast<unsigned char>(c)) || signedNumber) {
            const size_t digits = signedNumber ? i + 1 : i;
            const bool hex = line[digits] == '0' && digits + 1 < line.length() &&
                             (line[digits + 1] == 'x' || line[digits + 1] == 'X');
            size_t j = i + 1;
            while (j < line.length()) {
                if (std::isalnum(static_cast<unsigned char>(line[j])) || line[j] == '.') {
                    j++;
                } else if (line[j] == '-' && !hex && (line[j - 1] == 'e' || line[j - 1] == 'E') &&
                           j + 1 < line.length() && std::isdigit(static_cast<unsigned char>(line[j + 1]))) {
                    j++;
                } else {
                    break;
                }
            }
            paint(i, j, COLOR_PAIR(2));
            i = j;
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t j = i + 1;
            while (j < line.length() && isWordChar(line[j])) {
                j++;
            }
            if (keywords.count(line.substr(i, j - i))) {
                paint(i, j, COLOR_PAIR(1) | A_BOLD);
            }
            i = j;
            continue;
        }

        i++;
    }

    return depth;
}

static void printHighlightedRow(
    int screenY,
    int startX,
    const std::string &displayText,
    int colOffset,
    int logicalLine,
    const std::function<bool(int, int)> &isSelected,
    const std::vector<chtype> &attrs
) {
    for (size_t k = 0; k < displayText.length() && startX + static_cast<int>(k) < COLS; ++k) {
        const size_t col = static_cast<size_t>(colOffset) + k;
        const chtype attr = col < attrs.size() ? attrs[col] : 0;
        printCharWithSelection(screenY, startX + static_cast<int>(k), displayText[k],
                               isSelected(logicalLine, static_cast<int>(col)), attr);
    }
}

} // namespace

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
        chtype attr = COLOR_PAIR(pair);
        if (isSave && saveFlash) {
            attr |= A_BOLD | A_REVERSE;
        }

        attron(attr);
        mvprintw(contentTop, toolOffset, "[ %s ]", buttons[b].c_str());
        attroff(attr);
        toolOffset += static_cast<int>(buttons[b].length()) + 5;
    }
    contentTop++;

    int contentHeight = LINES - contentTop - 1;
    int lineNumWidth = (doc.mode == TuiMode::Code) ? 6 : 0;
    int wrapWidth = effectiveWrapWidth(doc, lineNumWidth);
    bool useWrap = (doc.wrapMode != TuiWrapMode::Off && wrapWidth > 0);

    const char *wrapLabel = (doc.wrapMode == TuiWrapMode::Off) ? "Off" :
        (doc.wrapMode == TuiWrapMode::Soft ? "Soft" : "Hard");

    TextPosition selStart = doc.cursor;
    TextPosition selEnd = doc.selectionAnchor;
    if (selEnd < selStart) {
        std::swap(selStart, selEnd);
    }

    auto isSelected = [&](int l, int c) -> bool {
        if (!doc.hasSelection) return false;
        if (l < selStart.line || l > selEnd.line) return false;
        if (l == selStart.line && l == selEnd.line) {
            return c >= selStart.col && c < selEnd.col;
        }
        if (l == selStart.line) return c >= selStart.col;
        if (l == selEnd.line) return c < selEnd.col;
        return true;
    };

    int runningDepth = 0;
    if (doc.mode == TuiMode::Code) {
        for (int l = 0; l < doc.scrollLine && l < static_cast<int>(doc.lines.size()); ++l) {
            runningDepth = scanCodeLine(doc.lines[l], runningDepth, nullptr);
        }
    }
    int scannedLine = -1;
    std::vector<chtype> lineAttrs;

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
        int colOffset = 0;

        if (useWrap) {
            if (!wrapStarts.empty()) {
                int displayRow = rowIndex;
                while (logicalLine + 1 < static_cast<int>(doc.lines.size()) && displayRow >= wrapStarts[logicalLine + 1]) {
                    ++logicalLine;
                }
                if (logicalLine >= static_cast<int>(doc.lines.size())) {
                    continue;
                }

                const auto segments = wrapDisplaySegmentsWithOffsets(doc.lines[logicalLine], wrapWidth);
                int segmentIndex = displayRow - wrapStarts[logicalLine];
                if (segmentIndex >= 0 && segmentIndex < static_cast<int>(segments.size())) {
                    displayText = segments[segmentIndex].text;
                    colOffset = segments[segmentIndex].colOffset;
                } else {
                    continue;
                }
            }
        } else {
            logicalLine = doc.scrollLine + y;
            if (logicalLine < static_cast<int>(doc.lines.size())) {
                const std::string &fullLine = doc.lines[logicalLine];
                if (doc.wrapMode == TuiWrapMode::Off && doc.scrollCol < static_cast<int>(fullLine.length())) {
                    displayText = fullLine.substr(doc.scrollCol);
                    colOffset = doc.scrollCol;
                } else if (doc.wrapMode == TuiWrapMode::Off) {
                    displayText = "";
                    colOffset = doc.scrollCol;
                } else {
                    displayText = fullLine;
                    colOffset = 0;
                }
            }
        }

        if (logicalLine >= static_cast<int>(doc.lines.size())) {
            continue;
        }

        int lastNonSpace = doc.lines[logicalLine].find_last_not_of(" \t");

        if (doc.mode == TuiMode::Code && logicalLine != scannedLine) {
            runningDepth = scanCodeLine(doc.lines[logicalLine], runningDepth, &lineAttrs);
            scannedLine = logicalLine;
        }

        if (doc.mode == TuiMode::Code) {
            attron(COLOR_PAIR(3));
            if (!useWrap) {
                mvprintw(screenY, 0, "%4d |", logicalLine + 1);
            } else {
                if (colOffset == 0) {
                    mvprintw(screenY, 0, "%4d |", logicalLine + 1);
                } else {
                    mvprintw(screenY, 0, "     |");
                }
            }
            attroff(COLOR_PAIR(3));
        }

        if (displayText.empty()) {
            continue;
        }

        if (doc.mode == TuiMode::Code) {
            printHighlightedRow(screenY, lineNumWidth, displayText, colOffset, logicalLine, isSelected, lineAttrs);
        } else {
            for (int x = 0; x < static_cast<int>(displayText.length()) && x + lineNumWidth < COLS; ++x) {
                int absCol = colOffset + x;
                bool isTrailing = (absCol > lastNonSpace || lastNonSpace == std::string::npos) && 
                                  (displayText[x] == ' ' || displayText[x] == '\t');
                bool sel = isSelected(logicalLine, absCol);

                chtype attr = isTrailing ? COLOR_PAIR(7) : 0;
                printCharWithSelection(screenY, lineNumWidth + x, displayText[x], sel, attr);
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
    int screenCursorX = lineNumWidth + std::min(doc.cursor.col, static_cast<int>(COLS - lineNumWidth - 1));

    if (useWrap) {
        int curLine = std::min(doc.cursor.line, static_cast<int>(doc.lines.size() - 1));
        const auto segments = wrapDisplaySegmentsWithOffsets(doc.lines[curLine], wrapWidth);
        int subRow = wrappedRowForCursor(segments, doc.cursor.col);
        int visualRow = (wrapStarts.empty() || curLine >= static_cast<int>(wrapStarts.size())) ? 0 : wrapStarts[curLine];
        visualRow += subRow;
        screenCursorY = contentTop + (visualRow - scrollBase);

        int colInSegment = doc.cursor.col;
        if (subRow < static_cast<int>(segments.size())) {
            colInSegment = doc.cursor.col - segments[subRow].colOffset;
        }
        screenCursorX = lineNumWidth + std::min(colInSegment, static_cast<int>(COLS - lineNumWidth - 1));
    } else {
        screenCursorX = lineNumWidth + std::max(0, doc.cursor.col - doc.scrollCol);
    }

    if (screenCursorY >= contentTop && screenCursorY < contentTop + contentHeight) {
        curs_set(1);
        move(screenCursorY, screenCursorX);
    } else {
        curs_set(0);
    }
    refresh();
}

} // namespace TuiView