#include "tuidialogs.h"
#include <curses.h>
#include <algorithm>
#include <string>
#include <vector>

namespace TuiDialogs {

static void drawDialogBorder(WINDOW *win, int height, int width) {
    // Explicit UTF-8 box characters: ┌ ─ ┐ │ └ ─ ┘
    // Bypasses VT100 alternate charset translation to prevent 'q' and 'x' characters
    mvwprintw(win, 0, 0, "\xe2\x94\x8c"); // ┌
    for (int i = 1; i < width - 1; ++i) {
        wprintw(win, "\xe2\x94\x80");     // ─
    }
    wprintw(win, "\xe2\x94\x90");         // ┐

    for (int y = 1; y < height - 1; ++y) {
        mvwprintw(win, y, 0, "\xe2\x94\x82");         // │
        mvwprintw(win, y, width - 1, "\xe2\x94\x82"); // │
    }

    mvwprintw(win, height - 1, 0, "\xe2\x94\x94"); // └
    for (int i = 1; i < width - 1; ++i) {
        wprintw(win, "\xe2\x94\x80");              // ─
    }
    wprintw(win, "\xe2\x94\x98");                  // ┘
}

std::string showPromptModal(const std::string &prompt) {
    int width = 54;
    int height = 5;
    int startY = (LINES - height) / 2;
    int startX = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, startY, startX);
    wbkgd(win, COLOR_PAIR(6));
    keypad(win, TRUE);
    drawDialogBorder(win, height, width);

    mvwprintw(win, 1, 2, "%s", prompt.c_str());

    std::string input;
    int maxLen = width - 6;

    while (true) {
        wmove(win, 2, 2);
        wclrtoeol(win);
        drawDialogBorder(win, height, width);

        int displayOffset = 0;
        if (static_cast<int>(input.length()) > maxLen) {
            displayOffset = static_cast<int>(input.length()) - maxLen;
        }

        std::string visibleText = input.substr(displayOffset, maxLen);
        mvwprintw(win, 2, 2, "%s", visibleText.c_str());
        wmove(win, 2, 2 + static_cast<int>(visibleText.length()));
        wrefresh(win);

        int ch = wgetch(win);

        if (ch == KEY_MOUSE) {
            MEVENT ev;
            getmouse(&ev);
            continue;
        }

        if (ch == 27) {
            delwin(win);
            return "";
        }

        if (ch == 10 || ch == KEY_ENTER) {
            delwin(win);
            return input;
        }

        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (!input.empty()) {
                input.pop_back();
            }
            continue;
        }

        if (ch >= 32 && ch <= 126) {
            input.push_back(static_cast<char>(ch));
        }
    }
}

int showConfirmModal(const std::string &title, const std::string &message) {
    int width = std::max(56, std::min(COLS - 4, static_cast<int>(message.length()) + 8));
    int height = 7;
    int startY = (LINES - height) / 2;
    int startX = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, startY, startX);
    wbkgd(win, COLOR_PAIR(6));
    keypad(win, TRUE);
    drawDialogBorder(win, height, width);

    mvwprintw(win, 1, (width - static_cast<int>(title.length())) / 2, "%s", title.c_str());
    mvwprintw(win, 3, (width - static_cast<int>(message.length())) / 2, "%s", message.c_str());
    mvwprintw(win, 5, (width - 34) / 2, "[Y] Save   [N] Discard   [Esc] Cancel");
    wrefresh(win);

    while (true) {
        int ch = wgetch(win);

        if (ch == 'y' || ch == 'Y' || ch == 10 || ch == KEY_ENTER) {
            delwin(win);
            return 1; // Save
        }
        if (ch == 'n' || ch == 'N' || ch == 'd' || ch == 'D') {
            delwin(win);
            return 2; // Discard / No
        }
        if (ch == 'c' || ch == 'C' || ch == 27) {
            delwin(win);
            return 0; // Cancel
        }

        if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK && (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
                int localY = ev.y - startY;
                int localX = ev.x - startX;
                if (localY == 5) {
                    int btnCenter = (width - 34) / 2;
                    // [Y] Save -> btnCenter to btnCenter + 9
                    if (localX >= btnCenter && localX <= btnCenter + 9) {
                        delwin(win);
                        return 1;
                    }
                    // [N] Discard -> btnCenter + 12 to btnCenter + 24
                    if (localX >= btnCenter + 12 && localX <= btnCenter + 24) {
                        delwin(win);
                        return 2;
                    }
                    // [Esc] Cancel -> btnCenter + 27 to btnCenter + 38
                    if (localX >= btnCenter + 27 && localX <= btnCenter + 38) {
                        delwin(win);
                        return 0;
                    }
                }
            }
        }
    }
}

bool showSaveAsModal(std::string &filePath, int &formatType) {
    int width = 60;
    int height = 9;
    int startY = (LINES - height) / 2;
    int startX = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, startY, startX);
    wbkgd(win, COLOR_PAIR(6));
    keypad(win, TRUE);

    int focusField = 0; // 0 = File name input, 1 = Format selector
    std::string inputName = filePath;
    int currentFmt = formatType; // 0 = HTML (.html), 1 = Markdown (.md)

    int maxInputLen = width - 16;

    while (true) {
        werase(win);
        drawDialogBorder(win, height, width);

        mvwprintw(win, 1, (width - 15) / 2, "Save Document As");

        // Row 3: File path / name input
        mvwprintw(win, 3, 3, "Name:   [");
        int dispOffset = 0;
        if (static_cast<int>(inputName.length()) > maxInputLen) {
            dispOffset = static_cast<int>(inputName.length()) - maxInputLen;
        }
        std::string dispName = inputName.substr(dispOffset, maxInputLen);
        if (focusField == 0) wattron(win, A_REVERSE);
        mvwprintw(win, 3, 12, "%-*s", maxInputLen, dispName.c_str());
        if (focusField == 0) wattroff(win, A_REVERSE);
        wprintw(win, "]");

        // Row 5: Format selector
        mvwprintw(win, 5, 3, "Format: ");
        if (focusField == 1) wattron(win, A_REVERSE);
        if (currentFmt == 0) {
            mvwprintw(win, 5, 12, "< HTML Document (*.html) >");
        } else {
            mvwprintw(win, 5, 12, "< Markdown File (*.md)   >");
        }
        if (focusField == 1) wattroff(win, A_REVERSE);

        // Row 7: Helper instructions
        mvwprintw(win, 7, 3, "Tab: Switch | Space/Arrows: Toggle | Enter: Save");

        wrefresh(win);
        int ch = wgetch(win);

        if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK && (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
                int localY = ev.y - startY;
                if (localY == 3) focusField = 0;
                else if (localY == 5) {
                    focusField = 1;
                    currentFmt = (currentFmt == 0) ? 1 : 0;
                }
            }
            continue;
        }

        if (ch == 27) { // Escape: cancel
            delwin(win);
            return false;
        }

        if (ch == '\t' || ch == KEY_DOWN) {
            focusField = (focusField == 0) ? 1 : 0;
            continue;
        }
        if (ch == KEY_BTAB || ch == KEY_UP) {
            focusField = (focusField == 1) ? 0 : 1;
            continue;
        }

        if (focusField == 1) {
            if (ch == ' ' || ch == KEY_LEFT || ch == KEY_RIGHT) {
                currentFmt = (currentFmt == 0) ? 1 : 0;
                continue;
            }
        }

        if (ch == 10 || ch == KEY_ENTER) {
            if (inputName.empty()) {
                inputName = (currentFmt == 0) ? "untitled.html" : "untitled.md";
            } else {
                std::string targetExt = (currentFmt == 0) ? ".html" : ".md";
                // Strip existing extension if mismatched or missing
                size_t dotPos = inputName.find_last_of('.');
                size_t slashPos = inputName.find_last_of("/\\");
                if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos)) {
                    inputName += targetExt;
                } else {
                    std::string curExt = inputName.substr(dotPos);
                    std::transform(curExt.begin(), curExt.end(), curExt.begin(), ::tolower);
                    if (curExt != ".html" && curExt != ".htm" && curExt != ".md" && curExt != ".txt" && curExt != ".cpp") {
                        inputName += targetExt;
                    } else if (currentFmt == 0 && (curExt == ".md")) {
                        inputName = inputName.substr(0, dotPos) + ".html";
                    } else if (currentFmt == 1 && (curExt == ".html" || curExt == ".htm")) {
                        inputName = inputName.substr(0, dotPos) + ".md";
                    }
                }
            }

            delwin(win);
            filePath = inputName;
            formatType = currentFmt;
            return true;
        }

        if (focusField == 0) {
            if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
                if (!inputName.empty()) inputName.pop_back();
            } else if (ch >= 32 && ch <= 126) {
                inputName.push_back(static_cast<char>(ch));
            }
        }
    }
}

bool showFindReplaceModal(std::string &findStr, std::string &replaceStr, bool &doReplace) {
    int width = 58;
    int height = 11;
    int startY = (LINES - height) / 2;
    int startX = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, startY, startX);
    wbkgd(win, COLOR_PAIR(6));
    keypad(win, TRUE);

    int focusField = 0; // 0 = Find input, 1 = Replace Checkbox, 2 = Replace input
    std::string findInput = findStr;
    std::string replaceInput = replaceStr;
    bool replaceEnabled = doReplace;

    int inputFieldWidth = width - 18;

    while (true) {
        werase(win);
        drawDialogBorder(win, height, width);

        mvwprintw(win, 1, (width - 16) / 2, "Find & Replace");

        // Row 3: Find input
        mvwprintw(win, 3, 3, "Find:    [");
        int findDispOffset = 0;
        if (static_cast<int>(findInput.length()) > inputFieldWidth) {
            findDispOffset = static_cast<int>(findInput.length()) - inputFieldWidth;
        }
        std::string dispFind = findInput.substr(findDispOffset, inputFieldWidth);
        if (focusField == 0) wattron(win, A_REVERSE);
        mvwprintw(win, 3, 13, "%-*s", inputFieldWidth, dispFind.c_str());
        if (focusField == 0) wattroff(win, A_REVERSE);
        wprintw(win, "]");

        // Row 5: Replace checkbox
        if (focusField == 1) wattron(win, A_REVERSE);
        mvwprintw(win, 5, 3, "[%c] Enable Replace Mode (Space to toggle)", replaceEnabled ? 'X' : ' ');
        if (focusField == 1) wattroff(win, A_REVERSE);

        // Row 7: Replace input
        mvwprintw(win, 7, 3, "Replace: [");
        if (!replaceEnabled) {
            wattron(win, A_DIM);
        } else if (focusField == 2) {
            wattron(win, A_REVERSE);
        }
        int repDispOffset = 0;
        if (static_cast<int>(replaceInput.length()) > inputFieldWidth) {
            repDispOffset = static_cast<int>(replaceInput.length()) - inputFieldWidth;
        }
        std::string dispRep = replaceEnabled ? replaceInput.substr(repDispOffset, inputFieldWidth) : "(Replace Disabled)";
        mvwprintw(win, 7, 13, "%-*s", inputFieldWidth, dispRep.c_str());
        if (!replaceEnabled) {
            wattroff(win, A_DIM);
        } else if (focusField == 2) {
            wattroff(win, A_REVERSE);
        }
        wprintw(win, "]");

        // Row 9: Instructions / shortcuts
        mvwprintw(win, 9, 3, "Tab: Next Field | Enter: %s | Esc: Cancel", replaceEnabled ? "Replace" : "Find Next");

        wrefresh(win);
        int ch = wgetch(win);

        if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK && (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
                int localY = ev.y - startY;
                if (localY == 3) focusField = 0;
                else if (localY == 5) {
                    focusField = 1;
                    replaceEnabled = !replaceEnabled;
                }
                else if (localY == 7 && replaceEnabled) focusField = 2;
            }
            continue;
        }

        if (ch == 27) { // Escape
            delwin(win);
            return false;
        }

        if (ch == '\t' || ch == KEY_DOWN) {
            if (focusField == 0) focusField = 1;
            else if (focusField == 1) focusField = replaceEnabled ? 2 : 0;
            else focusField = 0;
            continue;
        }

        if (ch == KEY_BTAB || ch == KEY_UP) {
            if (focusField == 2) focusField = 1;
            else if (focusField == 1) focusField = 0;
            else focusField = replaceEnabled ? 2 : 1;
            continue;
        }

        if (focusField == 1 && ch == ' ') {
            replaceEnabled = !replaceEnabled;
            continue;
        }

        if (ch == 10 || ch == KEY_ENTER) {
            delwin(win);
            findStr = findInput;
            replaceStr = replaceInput;
            doReplace = replaceEnabled;
            return !findStr.empty();
        }

        // Typing into input fields
        if (focusField == 0) {
            if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
                if (!findInput.empty()) findInput.pop_back();
            } else if (ch >= 32 && ch <= 126) {
                findInput.push_back(static_cast<char>(ch));
            }
        } else if (focusField == 2 && replaceEnabled) {
            if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
                if (!replaceInput.empty()) replaceInput.pop_back();
            } else if (ch >= 32 && ch <= 126) {
                replaceInput.push_back(static_cast<char>(ch));
            }
        }
    }
}

int showListSelectionModal(const std::string &title, const std::vector<std::string> &items) {
    int maxItemLen = static_cast<int>(title.length());
    for (const auto &item : items) {
        if (static_cast<int>(item.length()) > maxItemLen) {
            maxItemLen = static_cast<int>(item.length());
        }
    }
    int width = std::max(56, std::min(COLS - 4, maxItemLen + 6));
    int maxVisible = std::max(4, std::min(14, LINES - 6));
    int height = maxVisible + 4;
    int startY = (LINES - height) / 2;
    int startX = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, startY, startX);
    wbkgd(win, COLOR_PAIR(6));
    keypad(win, TRUE);

    int selected = 0;
    int scrollOffset = 0;

    #if defined(NCURSES_MOUSE_VERSION) && (NCURSES_MOUSE_VERSION > 1)
    const mmask_t wheelUp = BUTTON4_PRESSED;
    const mmask_t wheelDown = BUTTON5_PRESSED;
    #else
    const mmask_t wheelUp = 0x10000;
    const mmask_t wheelDown = 0x200000;
    #endif

    while (true) {
        werase(win);
        drawDialogBorder(win, height, width);

        mvwprintw(win, 1, (width - static_cast<int>(title.length())) / 2, "%s", title.c_str());

        if (selected < scrollOffset) {
            scrollOffset = selected;
        } else if (selected >= scrollOffset + maxVisible) {
            scrollOffset = selected - maxVisible + 1;
        }

        for (int row = 0; row < maxVisible; ++row) {
            int itemIdx = scrollOffset + row;
            if (itemIdx >= static_cast<int>(items.size())) break;

            if (itemIdx == selected) wattron(win, A_REVERSE);
            std::string itemText = items[itemIdx];
            if (static_cast<int>(itemText.length()) > width - 6) {
                itemText = itemText.substr(0, width - 9) + "...";
            }
            mvwprintw(win, 2 + row, 2, "%-*s", width - 4, itemText.c_str());
            if (itemIdx == selected) wattroff(win, A_REVERSE);
        }

        if (scrollOffset > 0) mvwaddch(win, 1, width - 2, '^');
        if (scrollOffset + maxVisible < static_cast<int>(items.size())) mvwaddch(win, height - 2, width - 2, 'v');

        wrefresh(win);
        int ch = wgetch(win);

        if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK) {
                if (ev.bstate & wheelUp) { // Wheel up
                    if (selected > 0) selected--;
                    continue;
                }
                if (ev.bstate & wheelDown) { // Wheel down
                    if (selected + 1 < static_cast<int>(items.size())) selected++;
                    continue;
                }
                if (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                    int clickedRow = ev.y - startY - 2;
                    if (clickedRow >= 0 && clickedRow < maxVisible) {
                        int itemIdx = scrollOffset + clickedRow;
                        if (itemIdx < static_cast<int>(items.size())) {
                            delwin(win);
                            return itemIdx;
                        }
                    }
                }
            }
            continue;
        }

        if (ch == KEY_UP && selected > 0) {
            selected--;
        } else if (ch == KEY_DOWN && selected + 1 < static_cast<int>(items.size())) {
            selected++;
        } else if (ch == KEY_PPAGE) {
            selected = std::max(0, selected - maxVisible);
        } else if (ch == KEY_NPAGE) {
            selected = std::min(static_cast<int>(items.size()) - 1, selected + maxVisible);
        } else if (ch == 10 || ch == KEY_ENTER) {
            delwin(win);
            return selected;
        } else if (ch == 27) {
            delwin(win);
            return -1;
        }
    }
}

void showAboutModal() {
    int width = 62;
    int height = 19;
    int startY = (LINES - height) / 2;
    int startX = (COLS - width) / 2;

    WINDOW *win = newwin(height, width, startY, startX);
    wbkgd(win, COLOR_PAIR(6));
    keypad(win, TRUE);
    drawDialogBorder(win, height, width);

    mvwprintw(win, 1, (width - 15) / 2, "MicroWriter TUI");
    mvwprintw(win, 3, 3, "F1: Help F2: New Tab F3: Cycle Tabs");
    mvwprintw(win, 4, 3, "F4/Ctrl+F: Find/Replace F5: Rich Layout F6: Code");
    mvwprintw(win, 5, 3, "F7/Ctrl+O: Open F8/Ctrl+S: Save F10: Export ODF");
    mvwprintw(win, 6, 3, "F11: Menu Bar F12/Ctrl+Q: Quit");

    mvwprintw(win, 8, 3, "Ctrl+X: Cut Ctrl+C: Copy Ctrl+V: Paste");
    mvwprintw(win, 9, 3, "Ctrl+A: Select All Ctrl+D: Cut Line Ctrl+Z/Ctrl+E: Undo");
    mvwprintw(win, 10, 3, "Ctrl+R: Redo Tab: 4-Spaces Indent");

    mvwprintw(win, 12, 3, "Tabs: Double-click = New Tab, Right-click = Close Menu");
    mvwprintw(win, 13, 3, "Clipboard: Native wl-copy / xclip bidirectional sync");
    mvwprintw(win, 14, 3, "Navigation: Arrows, PgUp, PgDn, Home, End");
    mvwprintw(win, 15, 3, "Cancel : Esc cancels prompts/menus");

    mvwprintw(win, 17, (width - 24) / 2, "[ Press Esc or Any Key ]");

    wrefresh(win);

    while (true) {
        int ch = wgetch(win);
        if (ch == KEY_MOUSE) {
            MEVENT ev;
            getmouse(&ev);
            continue;
        }
        break;
    }

    delwin(win);
}

int showRegexCheatSheetModal(std::string &outFind, std::string &outReplace) {
    struct PatternItem {
        std::string desc;
        std::string find;
        std::string rep;
    };
    static const std::vector<PatternItem> patterns = {
        {"Cut everything after char 'r'", "(r).*", "$1"},
        {"Cut everything before 'Ska'", ".*(Ska)", "$1"},
        {"Keep characters inside quotes", "\"([^\"]*)\"", "$1"},
        {"Remove all digits / numbers", "\\d+", ""},
        {"Remove leading whitespace", "^[ \\t]+", ""},
        {"Remove trailing whitespace", "[ \\t]+$", ""},
        {"Multiple spaces to single space", "[ \\t]+", " "},
        {"Extract email addresses", "([a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+)", "$1"},
        {"Convert Markdown headers (# )", "^#+\\s*(.*)$", "$1"}
    };

    std::vector<std::string> displayList;
    for (const auto &p : patterns) {
        displayList.push_back(p.desc + " | " + p.find);
    }

    int sel = showListSelectionModal("Regex Patterns (Enter to Load)", displayList);
    if (sel >= 0 && sel < static_cast<int>(patterns.size())) {
        outFind = patterns[sel].find;
        outReplace = patterns[sel].rep;
        return sel;
    }
    return -1;
}

int showHtmlCheatSheetModal(std::string &outTag) {
    struct TagItem {
        std::string desc;
        std::string tag;
    };
    static const std::vector<TagItem> snippets = {
        {"Paragraph", "<p></p>"},
        {"Heading 1", "<h1></h1>"},
        {"Heading 2", "<h2></h2>"},
        {"Bold text", "<b></b>"},
        {"Italic text", "<i></i>"},
        {"Anchor / Link", "<a href=\"\"></a>"},
        {"Image element", "<img src=\"\" alt=\"\" />"},
        {"Unordered List", "<ul>\n  <li></li>\n</ul>"},
        {"Ordered List", "<ol>\n  <li></li>\n</ol>"},
        {"Blockquote", "<blockquote></blockquote>"},
        {"Inline Code", "<code></code>"},
        {"Code Block", "<pre><code></code></pre>"},
        {"Horizontal Rule", "<hr />"}
    };

    std::vector<std::string> displayList;
    for (const auto &s : snippets) {
        displayList.push_back(s.desc + " -> " + s.tag);
    }

    int sel = showListSelectionModal("HTML Snippets (Enter to Insert)", displayList);
    if (sel >= 0 && sel < static_cast<int>(snippets.size())) {
        outTag = snippets[sel].tag;
        return sel;
    }
    return -1;
}

int showSedUtilsCheatSheetModal(std::string &outSnippet) {
    struct SedItem {
        std::string desc;
        std::string cmd;
    };
    static const std::vector<SedItem> items = {
        {"sed: Replace first occurrence per line", "sed 's/foo/bar/' file.txt"},
        {"sed: Global search and replace", "sed 's/foo/bar/g' file.txt"},
        {"sed: In-place file replace", "sed -i 's/foo/bar/g' file.txt"},
        {"sed: Delete empty lines", "sed '/^$/d' file.txt"},
        {"sed: Delete lines matching pattern", "sed '/pattern/d' file.txt"},
        {"sed: Print specific line numbers (e.g. 5-10)", "sed -n '5,10p' file.txt"},
        {"awk: Print specific column (col 1)", "awk '{print $1}' file.txt"},
        {"awk: Sum values in column 1", "awk '{s+=$1} END {print s}' file.txt"},
        {"cut: Extract delimiter separated fields (CSV)", "cut -d',' -f1,3 file.csv"},
        {"grep: Recursive case-insensitive with line numbers", "grep -rnI 'search_term' ."},
        {"tr: Translate uppercase to lowercase", "cat file.txt | tr '[:upper:]' '[:lower:]'"},
        {"xargs: Parallel command execution (4 jobs)", "cat files.txt | xargs -n 1 -P 4 command"}
    };

    std::vector<std::string> displayList;
    for (const auto &it : items) {
        displayList.push_back(it.desc + " | " + it.cmd);
    }

    int sel = showListSelectionModal("Sed & Unix Utilities Cheat Sheet (Enter to Insert)", displayList);
    if (sel >= 0 && sel < static_cast<int>(items.size())) {
        outSnippet = items[sel].cmd;
        return sel;
    }
    return -1;
}

int showBashScriptCheatSheetModal(std::string &outScript) {
    struct ScriptItem {
        std::string title;
        std::string code;
    };
    static const std::vector<ScriptItem> scripts = {
        {
            "Strict Mode Boilerplate (set -euo pipefail)",
            "#!/usr/bin/env bash\nset -euo pipefail\nIFS=$'\\n\\t'\n\n# Main entry point\nmain() {\n  echo \"Starting script...\"\n}\nmain \"$@\"\n"
        },
        {
            "CLI Option & Argument Parser (getopts)",
            "while getopts \":f:h\" opt; do\n  case ${opt} in\n    f ) FILE=\"$OPTARG\" ;;\n    h ) echo \"Usage: $0 [-f file]\"; exit 0 ;;\n    \\? ) echo \"Invalid Option: -$OPTARG\" 1>&2; exit 1 ;;\n  esac\ndone\n"
        },
        {
            "Traverse Directory & Loop Over Files Safe",
            "while IFS= read -r -d '' file; do\n  echo \"Processing: $file\"\ndone < <(find . -type f -name \"*.cpp\" -print0)\n"
        },
        {
            "Temporary Directory with Auto-Cleanup Trap",
            "TMP_DIR=$(mktemp -d)\ntrap 'rm -rf \"$TMP_DIR\"' EXIT INT TERM\necho \"Using temporary directory: $TMP_DIR\"\n"
        },
        {
            "Check Command Existence / Prerequisite Guard",
            "command -v jq >/dev/null 2>&1 || { echo \"Error: 'jq' is required but not installed.\" >&2; exit 1; }\n"
        },
        {
            "Prompt User for Confirmation (y/N)",
            "read -r -p \"Are you sure you want to proceed? [y/N] \" response\ncase \"$response\" in\n  [yY][eE][sS]|[yY]) echo \"Confirmed.\" ;;\n  *) echo \"Aborted.\"; exit 1 ;;\nesac\n"
        }
    };

    std::vector<std::string> displayList;
    for (const auto &s : scripts) {
        displayList.push_back(s.title);
    }

    int sel = showListSelectionModal("Useful Bash Scripts (Enter to Insert)", displayList);
    if (sel >= 0 && sel < static_cast<int>(scripts.size())) {
        outScript = scripts[sel].code;
        return sel;
    }
    return -1;
}

int showTabContextMenuModal(const std::string &tabTitle) {
    std::vector<std::string> options = {
        "Save File",
        "Close Tab",
        "Save and Close Tab",
        "Cancel"
    };
    int sel = showListSelectionModal("Tab: " + tabTitle, options);
    if (sel == 0) return 1; // Save
    if (sel == 1) return 2; // Close
    if (sel == 2) return 3; // Save & Close
    return 0; // Cancel
}

} // namespace TuiDialogs
