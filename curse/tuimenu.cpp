#include "tuimenu.h"
#include "tuiview.h"
#include <curses.h>
#include <algorithm>

namespace TuiMenu {

static void drawMenuBorder(WINDOW *win, int height, int width) {
    // UTF-8 box characters: ┌ ─ ┐ │ └ ─ ┘
    // Directly output UTF-8 so terminals never interpret them as VT100 ACS 'q' and 'x'
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

void runMenuLoop(
    std::vector<EditorDocument> &documents,
    size_t &activeDocIdx,
    int &initialMenuSelect,
    const std::vector<std::string> &topMenus,
    const std::vector<std::vector<std::string>> &subMenus,
    std::function<void(int menuIdx, int itemIdx)> onAction
) {
    bool menuActive = true;
    int menuSelect = initialMenuSelect;
    int subSelect = 0;

    #if defined(NCURSES_MOUSE_VERSION) && (NCURSES_MOUSE_VERSION > 1)
    const mmask_t wheelUp = BUTTON4_PRESSED;
    const mmask_t wheelDown = BUTTON5_PRESSED;
    #else
    const mmask_t wheelUp = 0x10000;
    const mmask_t wheelDown = 0x200000;
    #endif

    while (menuActive) {
        TuiView::render(documents, activeDocIdx, true, menuSelect, topMenus);

        int subX = 2;
        for (int i = 0; i < menuSelect; ++i) {
            subX += static_cast<int>(topMenus[i].length()) + 4;
        }

        const auto &items = subMenus[menuSelect];
        int subHeight = static_cast<int>(items.size()) + 2;

        int maxItemLen = 0;
        for (const auto &item : items) {
            if (static_cast<int>(item.length()) > maxItemLen) {
                maxItemLen = static_cast<int>(item.length());
            }
        }
        int subWidth = std::max(26, maxItemLen + 4);

        if (subX + subWidth >= COLS) {
            subX = std::max(0, COLS - subWidth - 1);
        }

        WINDOW *mWin = newwin(subHeight, subWidth, 1, subX);
        wbkgd(mWin, COLOR_PAIR(6));
        drawMenuBorder(mWin, subHeight, subWidth);

        for (size_t j = 0; j < items.size(); ++j) {
            if (static_cast<int>(j) == subSelect) wattron(mWin, A_REVERSE);
            mvwprintw(mWin, 1 + static_cast<int>(j), 2, "%-*s", subWidth - 4, items[j].c_str());
            if (static_cast<int>(j) == subSelect) wattroff(mWin, A_REVERSE);
        }
        wrefresh(mWin);

        int ch = getch();

        if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK) {
                // Wheel scrolling inside menu
                if (ev.bstate & wheelUp) {
                    subSelect = (subSelect + static_cast<int>(items.size()) - 1) % items.size();
                    delwin(mWin);
                    continue;
                }
                if (ev.bstate & wheelDown) {
                    subSelect = (subSelect + 1) % items.size();
                    delwin(mWin);
                    continue;
                }

                // Top menu bar interaction
                if (ev.y == 0) {
                    int offset = 2;
                    bool hit = false;
                    for (size_t i = 0; i < topMenus.size(); ++i) {
                        int menuLen = static_cast<int>(topMenus[i].length()) + 4;
                        if (ev.x >= offset && ev.x < offset + menuLen) {
                            if (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                                if (menuSelect == static_cast<int>(i)) menuActive = false;
                                else { menuSelect = static_cast<int>(i); subSelect = 0; }
                            } else if (ev.bstate & REPORT_MOUSE_POSITION) {
                                if (menuSelect != static_cast<int>(i)) {
                                    menuSelect = static_cast<int>(i);
                                    subSelect = 0;
                                }
                            }
                            hit = true;
                            break;
                        }
                        offset += menuLen;
                    }
                    if (!hit && (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
                        menuActive = false;
                    }
                } 
                // Submenu items interaction (click or hover tracking)
                else if (ev.y >= 1 && ev.y < 1 + subHeight && ev.x >= subX && ev.x < subX + subWidth) {
                    int itemIdx = ev.y - 2;
                    if (itemIdx >= 0 && itemIdx < static_cast<int>(items.size())) {
                        subSelect = itemIdx;
                        if (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                            delwin(mWin);
                            onAction(menuSelect, itemIdx);
                            return;
                        }
                    }
                } 
                // Click outside closes menu
                else if (ev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                    menuActive = false;
                }
            }
        } else if (ch == KEY_LEFT) {
            menuSelect = (menuSelect + static_cast<int>(topMenus.size()) - 1) % topMenus.size();
            subSelect = 0;
        } else if (ch == KEY_RIGHT) {
            menuSelect = (menuSelect + 1) % topMenus.size();
            subSelect = 0;
        } else if (ch == KEY_DOWN) {
            subSelect = (subSelect + 1) % items.size();
        } else if (ch == KEY_UP) {
            subSelect = (subSelect + static_cast<int>(items.size()) - 1) % items.size();
        } else if (ch == 10 || ch == KEY_ENTER) {
            delwin(mWin);
            onAction(menuSelect, subSelect);
            return;
        } else if (ch == 27 || ch == KEY_F(10) || ch == KEY_F(11)) {
            menuActive = false;
        }

        delwin(mWin);
    }
}

} // namespace TuiMenu
