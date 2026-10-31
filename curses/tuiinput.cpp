#include "tuiinput.h"
#include "tuiactions.h"
#include "tuidialogs.h"
#include "tuimenu.h"
#include "tuiview.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <string>

namespace TuiInput {

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

} // namespace

void handleMouseEvent(const MEVENT &event, TuiEditorContext &ctx) {
#if defined(NCURSES_MOUSE_VERSION) && (NCURSES_MOUSE_VERSION > 1)
    const mmask_t wheelUp = BUTTON4_PRESSED;
    const mmask_t wheelDown = BUTTON5_PRESSED;
#else
    const mmask_t wheelUp = 0x10000;
    const mmask_t wheelDown = 0x200000;
#endif

    auto &doc = ctx.doc();

    if (event.bstate & wheelUp) {
        int scrollStep = 3;
        doc.scrollLine = std::max(0, doc.scrollLine - scrollStep);
        int contentHeight = std::max(1, LINES - 4);
        if (doc.cursor.line > doc.scrollLine + contentHeight - 1) {
            doc.cursor.line = doc.scrollLine + contentHeight - 1;
            doc.cursor.col = std::min(doc.cursor.col, static_cast<int>(doc.lines[doc.cursor.line].length()));
        }
        return;
    }
    if (event.bstate & wheelDown) {
        int maxScroll = std::max(0, static_cast<int>(doc.lines.size()) - 1);
        int scrollStep = 3;
        doc.scrollLine = std::min(maxScroll, doc.scrollLine + scrollStep);
        if (doc.cursor.line < doc.scrollLine) {
            doc.cursor.line = doc.scrollLine;
            doc.cursor.col = std::min(doc.cursor.col, static_cast<int>(doc.lines[doc.cursor.line].length()));
        }
        return;
    }

    int my = event.y, mx = event.x;

    if (my == 0 && (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
        int offset = 2;
        for (size_t i = 0; i < ctx.topMenus.size(); ++i) {
            int menuLen = static_cast<int>(ctx.topMenus[i].length()) + 4;
            if (mx >= offset && mx < offset + menuLen) {
                int sel = static_cast<int>(i);
                TuiMenu::runMenuLoop(ctx.documents, ctx.activeDocIdx, sel, ctx.topMenus, ctx.subMenus,
                                     [&ctx](int m, int it) { ctx.executeMenuAction(m, it); });
                return;
            }
            offset += menuLen;
        }
        return;
    }

    if (my == 1) {
        int tabOffset = 1;
        int clickedTab = -1;
        for (size_t i = 0; i < ctx.documents.size(); ++i) {
            int tabLen = static_cast<int>(ctx.documents[i].title.length()) + 8;
            if (mx >= tabOffset && mx < tabOffset + tabLen) {
                clickedTab = static_cast<int>(i);
                break;
            }
            tabOffset += tabLen;
        }

        if ((event.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) && clickedTab >= 0) {
            int res = TuiDialogs::showTabContextMenuModal(ctx.documents[clickedTab].title);
            if (res == 1) {
                size_t oldActive = ctx.activeDocIdx;
                ctx.activeDocIdx = clickedTab;
                ctx.saveCurrentFile();
                ctx.activeDocIdx = oldActive;
            } else if (res == 2) {
                ctx.closeDocument(clickedTab);
            } else if (res == 3) {
                size_t oldActive = ctx.activeDocIdx;
                ctx.activeDocIdx = clickedTab;
                ctx.saveCurrentFile();
                ctx.activeDocIdx = oldActive;
                ctx.closeDocument(clickedTab);
            }
            return;
        }

        if (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx.lastClickTime).count();

            if (clickedTab >= 0) {
                if (clickedTab == ctx.lastClickTabIdx && elapsed < 350) {
                    EditorDocument d;
                    if (ctx.defaultCodeMode) d.mode = TuiMode::Code;
                    ctx.documents.push_back(d);
                    ctx.activeDocIdx = ctx.documents.size() - 1;
                    ctx.lastClickTabIdx = -1;
                    return;
                }
                ctx.activeDocIdx = clickedTab;
                ctx.lastClickTabIdx = clickedTab;
                ctx.lastClickTime = now;
                return;
            } else {
                if (ctx.lastClickTabIdx == -2 && elapsed < 350) {
                    EditorDocument d;
                    if (ctx.defaultCodeMode) d.mode = TuiMode::Code;
                    ctx.documents.push_back(d);
                    ctx.activeDocIdx = ctx.documents.size() - 1;
                    ctx.lastClickTabIdx = -1;
                    return;
                }
                ctx.lastClickTabIdx = -2;
                ctx.lastClickTime = now;
            }
            return;
        }
    }

    if (my == 2 && (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
        int toolOffset = 1;
        bool isMd = TuiActions::isMarkdownDoc(doc);

        if (doc.mode == TuiMode::RichText) {
            for (size_t b = 0; b < TuiView::toolbarButtons.size(); ++b) {
                int btnLen = static_cast<int>(TuiView::toolbarButtons[b].length()) + 4;
                if (mx >= toolOffset && mx < toolOffset + btnLen) {
                    if (b == 0) ctx.saveCurrentFile();
                    else if (b == 1) ctx.promptOpen();
                    else if (b == 2) {
                        if (!isMd) TuiActions::handleFontSelection(doc);
                    }
                    else if (b == 3) {
                        if (!isMd) TuiActions::handleFontSizeSelection(doc);
                    }
                    else if (b == 4) TuiActions::applyBold(doc);
                    else if (b == 5) TuiActions::applyItalic(doc);
                    return;
                }
                toolOffset += btnLen + 1;
            }
        } else {
            for (size_t b = 0; b < TuiView::codeButtons.size(); ++b) {
                int btnLen = static_cast<int>(TuiView::codeButtons[b].length()) + 4;
                if (mx >= toolOffset && mx < toolOffset + btnLen) {
                    if (b == 0) ctx.saveCurrentFile();
                    else if (b == 1) ctx.promptOpen();
                    else if (b == 2) TuiActions::handleToggleComment(doc);
                    else if (b == 3) TuiActions::handleDuplicate(doc);
                    else if (b == 4) TuiActions::handleIndent(doc, true);
                    else if (b == 5) TuiActions::handleIndent(doc, false);
                    return;
                }
                toolOffset += btnLen + 1;
            }
        }
        return;
    }

    int contentTop = 3;
    int contentBottom = LINES - 2;
    if ((my >= contentTop && my <= contentBottom) || ctx.isDragging) {
        int lineNumWidth = (doc.mode == TuiMode::Code) ? 6 : 0;
        int clampedY = std::clamp(my, contentTop, contentBottom);

        int wrapWidth = effectiveWrapWidth(doc, lineNumWidth);
        bool useWrap = (doc.wrapMode != TuiWrapMode::Off && wrapWidth > 0);

        int targetLine = 0;
        int targetCol = 0;

        if (useWrap) {
            auto wrapStarts = buildWrapStarts(doc.lines, wrapWidth);
            int scrollBase = doc.scrollLine;
            if (!wrapStarts.empty() && static_cast<size_t>(doc.scrollLine) < wrapStarts.size()) {
                scrollBase = wrapStarts[std::min(doc.scrollLine, static_cast<int>(wrapStarts.size() - 1))];
            }

            int displayRow = scrollBase + (clampedY - contentTop);
            targetLine = doc.scrollLine;

            if (!wrapStarts.empty()) {
                while (targetLine + 1 < static_cast<int>(doc.lines.size()) && displayRow >= wrapStarts[targetLine + 1]) {
                    ++targetLine;
                }
            }
            targetLine = std::clamp(targetLine, 0, static_cast<int>(doc.lines.size()) - 1);

            const auto segments = wrapDisplaySegmentsWithOffsets(doc.lines[targetLine], wrapWidth);
            int segmentIdx = (!wrapStarts.empty() && targetLine < static_cast<int>(wrapStarts.size()))
                                 ? (displayRow - wrapStarts[targetLine])
                                 : 0;
            segmentIdx = std::clamp(segmentIdx, 0, std::max(0, static_cast<int>(segments.size()) - 1));

            int clickedColInSegment = std::max(0, mx - lineNumWidth);
            if (!segments.empty()) {
                int segLen = static_cast<int>(segments[segmentIdx].text.length());
                clickedColInSegment = std::min(clickedColInSegment, segLen);
                targetCol = segments[segmentIdx].colOffset + clickedColInSegment;
            } else {
                targetCol = 0;
            }
            targetCol = std::min(targetCol, static_cast<int>(doc.lines[targetLine].length()));
        } else {
            targetLine = doc.scrollLine + (clampedY - contentTop);
            targetLine = std::clamp(targetLine, 0, static_cast<int>(doc.lines.size()) - 1);
            int clickedColOffset = std::max(0, mx - lineNumWidth);
            targetCol = doc.wrapMode == TuiWrapMode::Off ? (doc.scrollCol + clickedColOffset) : clickedColOffset;
            targetCol = std::min(targetCol, static_cast<int>(doc.lines[targetLine].length()));
        }

        if (doc.mode == TuiMode::Code && mx < lineNumWidth) {
            if (doc.hasSelection && (event.bstate & BUTTON1_PRESSED)) {
                int startLine = std::min(doc.selectionAnchor.line, targetLine);
                int endLine = std::max(doc.selectionAnchor.line, targetLine);
                
                doc.selectionAnchor.line = startLine;
                doc.selectionAnchor.col = 0;
                doc.cursor.line = endLine;
                doc.cursor.col = static_cast<int>(doc.lines[endLine].length());
                doc.hasSelection = (startLine != endLine || doc.selectionAnchor.col != doc.cursor.col);
            } else if (event.bstate & BUTTON1_PRESSED) {
                doc.selectionAnchor.line = targetLine;
                doc.selectionAnchor.col = 0;
                doc.cursor.line = targetLine;
                doc.cursor.col = static_cast<int>(doc.lines[targetLine].length());
                doc.hasSelection = true;
            }
            return;
        }

        if (event.bstate & BUTTON1_PRESSED) {
            doc.cursor.line = targetLine;
            doc.cursor.col = targetCol;
            doc.selectionAnchor = doc.cursor;
            doc.hasSelection = false;
            ctx.isDragging = true;
        } else if ((event.bstate & REPORT_MOUSE_POSITION) && ctx.isDragging) {
            doc.cursor.line = targetLine;
            doc.cursor.col = targetCol;
            if (!(doc.cursor == doc.selectionAnchor)) doc.hasSelection = true;
        } else if (event.bstate & BUTTON1_RELEASED) {
            if (ctx.isDragging) {
                doc.cursor.line = targetLine;
                doc.cursor.col = targetCol;
                if (!(doc.cursor == doc.selectionAnchor)) doc.hasSelection = true;
            }
            ctx.isDragging = false;
        }
        return;
    }
}

void handleInput(int ch, TuiEditorContext &ctx) {
    if (ch == KEY_MOUSE) {
        MEVENT ev;
        if (getmouse(&ev) == OK) handleMouseEvent(ev, ctx);
        return;
    }

    auto &doc = ctx.doc();

    if (ch == KEY_F(1)) { TuiDialogs::showAboutModal(); return; }
    if (ch == KEY_F(2)) {
        EditorDocument d;
        if (ctx.defaultCodeMode) d.mode = TuiMode::Code;
        ctx.documents.push_back(d);
        ctx.activeDocIdx = ctx.documents.size() - 1;
        return;
    }
    if (ch == KEY_F(3)) {
        if (ctx.documents.size() > 1) {
            ctx.activeDocIdx = (ctx.activeDocIdx + 1) % ctx.documents.size();
        }
        return;
    }
    if (ch == KEY_F(4) || ch == 6) {
        TuiActions::handleFindReplace(doc);
        return;
    }
    if (ch == KEY_F(5)) { ctx.switchDocumentMode(TuiMode::RichText); return; }
    if (ch == KEY_F(6)) { ctx.switchDocumentMode(TuiMode::Code); return; }
    if (ch == KEY_F(7) || ch == 15) {
        ctx.promptOpen();
        return;
    }
    if (ch == KEY_F(8) || ch == 19) {
        ctx.saveCurrentFile();
        return;
    }
    if (ch == KEY_F(9)) {
        TuiActions::handleSpellCheck(doc);
        return;
    }
    if (ch == KEY_F(10)) { ctx.exportDocument("ODF", "odt"); return; }
    if (ch == KEY_F(11)) {
        int sel = 0;
        TuiMenu::runMenuLoop(ctx.documents, ctx.activeDocIdx, sel, ctx.topMenus, ctx.subMenus,
                             [&ctx](int m, int it) { ctx.executeMenuAction(m, it); });
        return;
    }
    if (ch == KEY_F(12) || ch == 17) { ctx.promptQuit(); return; }

    if (ch == 27) {
        nodelay(stdscr, TRUE);
        int nextCh = getch();
        nodelay(stdscr, FALSE);
        if (nextCh == 's' || nextCh == 'S') {
            TuiActions::handleSpellCheck(doc);
            return;
        }
        if (nextCh != ERR) {
            ungetch(nextCh);
        }
    }

    if (ch == 3) { TuiActions::handleCutCopy(doc, false); return; }
    if (ch == 24) { TuiActions::handleCutCopy(doc, true); return; }
    if (ch == 4) {
        doc.hasSelection = false;
        doc.pushUndo();
        TuiActions::handleCutCopy(doc, true);
        doc.modified = true;
        return;
    }
    if (ch == 22) { TuiActions::handlePaste(doc); return; }
    if (ch == 1) {
        doc.selectionAnchor = {0, 0};
        doc.cursor.line = static_cast<int>(doc.lines.size()) - 1;
        doc.cursor.col = static_cast<int>(doc.lines.back().length());
        doc.hasSelection = true;
        return;
    }
    if (ch == 18) { TuiActions::handleRedo(doc); return; }
    if (ch == 26 || ch == 5) {
        TuiActions::handleUndo(doc);
        return;
    }

    if (ch == 9) {
        if (doc.hasSelection) {
            TuiActions::handleIndent(doc, true);
        } else {
            doc.pushUndo();
            doc.lines[doc.cursor.line].insert(doc.cursor.col, "    ");
            doc.cursor.col += 4;
            doc.modified = true;
        }
        return;
    }

    if (ch == KEY_BTAB) {
        TuiActions::handleIndent(doc, false);
        return;
    }

    if (ch == KEY_UP) {
        if (doc.cursor.line > 0) doc.cursor.line--;
        doc.cursor.col = std::min(doc.cursor.col, static_cast<int>(doc.lines[doc.cursor.line].length()));
    } else if (ch == KEY_DOWN) {
        if (doc.cursor.line + 1 < static_cast<int>(doc.lines.size())) doc.cursor.line++;
        doc.cursor.col = std::min(doc.cursor.col, static_cast<int>(doc.lines[doc.cursor.line].length()));
    } else if (ch == KEY_LEFT) {
        if (doc.cursor.col > 0) doc.cursor.col--;
        else if (doc.cursor.line > 0) {
            doc.cursor.line--;
            doc.cursor.col = static_cast<int>(doc.lines[doc.cursor.line].length());
        }
    } else if (ch == KEY_RIGHT) {
        if (doc.cursor.col < static_cast<int>(doc.lines[doc.cursor.line].length())) doc.cursor.col++;
        else if (doc.cursor.line + 1 < static_cast<int>(doc.lines.size())) {
            doc.cursor.line++;
            doc.cursor.col = 0;
        }
    } else if (ch == KEY_PPAGE) {
        int pageJump = std::max(1, LINES - 4);
        doc.cursor.line = std::max(0, doc.cursor.line - pageJump);
        doc.cursor.col = std::min(doc.cursor.col, static_cast<int>(doc.lines[doc.cursor.line].length()));
    } else if (ch == KEY_NPAGE) {
        int pageJump = std::max(1, LINES - 4);
        doc.cursor.line = std::min(static_cast<int>(doc.lines.size()) - 1, doc.cursor.line + pageJump);
        doc.cursor.col = std::min(doc.cursor.col, static_cast<int>(doc.lines[doc.cursor.line].length()));
    } else if (ch == KEY_HOME) {
        doc.cursor.col = 0;
    } else if (ch == KEY_END) {
        doc.cursor.col = static_cast<int>(doc.lines[doc.cursor.line].length());
    } else if (ch == KEY_DC) {
        if (doc.hasSelection) {
            TuiActions::handleDeleteSelection(doc);
            return;
        }
        doc.pushUndo();
        if (doc.cursor.col < static_cast<int>(doc.lines[doc.cursor.line].length())) {
            doc.lines[doc.cursor.line].erase(doc.cursor.col, 1);
            doc.modified = true;
        } else if (doc.cursor.line + 1 < static_cast<int>(doc.lines.size())) {
            doc.lines[doc.cursor.line] += doc.lines[doc.cursor.line + 1];
            doc.lines.erase(doc.lines.begin() + doc.cursor.line + 1);
            doc.modified = true;
        }
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
        if (doc.hasSelection) {
            TuiActions::handleDeleteSelection(doc);
            return;
        }
        if (doc.mode == TuiMode::Code && TuiActions::handleSmartBackspace(doc)) {
            return;
        }

        if (doc.mode == TuiMode::RichText) {
            std::string &cur = doc.lines[doc.cursor.line];
            if (cur.rfind("• ", 0) == 0 && doc.cursor.col <= static_cast<int>(std::string("• ").length())) {
                doc.pushUndo();
                cur.erase(0, std::string("• ").length());
                doc.cursor.col = 0;
                doc.modified = true;
                return;
            }
            size_t dotPos = cur.find(". ");
            if (dotPos != std::string::npos && dotPos > 0 && dotPos < 6) {
                bool allDigits = true;
                for (size_t di = 0; di < dotPos; ++di) {
                    if (!std::isdigit(static_cast<unsigned char>(cur[di]))) {
                        allDigits = false;
                        break;
                    }
                }
                if (allDigits && doc.cursor.col <= static_cast<int>(dotPos + 2)) {
                    doc.pushUndo();
                    cur.erase(0, dotPos + 2);
                    doc.cursor.col = 0;
                    doc.modified = true;
                    return;
                }
            }
        }

        doc.pushUndo();
        if (doc.cursor.col > 0) {
            doc.lines[doc.cursor.line].erase(doc.cursor.col - 1, 1);
            doc.cursor.col--;
            doc.modified = true;
        } else if (doc.cursor.line > 0) {
            int prevLen = static_cast<int>(doc.lines[doc.cursor.line - 1].length());
            doc.lines[doc.cursor.line - 1] += doc.lines[doc.cursor.line];
            doc.lines.erase(doc.lines.begin() + doc.cursor.line);
            doc.cursor.line--;
            doc.cursor.col = prevLen;
            doc.modified = true;
        }
    } else if (ch == 10 || ch == KEY_ENTER) {
        if (doc.hasSelection) {
            TuiActions::handleDeleteSelection(doc);
        }
        if (doc.mode == TuiMode::Code) {
            TuiActions::handleSmartReturn(doc);
        } else {
            doc.pushUndo();
            std::string cur = doc.lines[doc.cursor.line];
            std::string rem = cur.substr(doc.cursor.col);
            std::string prefix = cur.substr(0, doc.cursor.col);

            if (prefix == "• ") {
                doc.lines[doc.cursor.line] = "";
                doc.cursor.col = 0;
                doc.modified = true;
                return;
            }

            size_t dotPos = prefix.find(". ");
            bool isNumbered = false;
            int currentNum = 0;
            if (dotPos != std::string::npos && dotPos > 0 && dotPos < 6) {
                bool allDigits = true;
                for (size_t di = 0; di < dotPos; ++di) {
                    if (!std::isdigit(static_cast<unsigned char>(prefix[di]))) {
                        allDigits = false;
                        break;
                    }
                }
                if (allDigits) {
                    isNumbered = true;
                    currentNum = std::atoi(prefix.substr(0, dotPos).c_str());
                    if (prefix == prefix.substr(0, dotPos + 2)) {
                        doc.lines[doc.cursor.line] = "";
                        doc.cursor.col = 0;
                        doc.modified = true;
                        return;
                    }
                }
            }

            std::string nextLinePrefix = "";
            if (prefix.rfind("• ", 0) == 0) {
                nextLinePrefix = "• ";
            } else if (isNumbered) {
                nextLinePrefix = std::to_string(currentNum + 1) + ". ";
            }

            doc.lines[doc.cursor.line] = prefix;
            doc.lines.insert(doc.lines.begin() + doc.cursor.line + 1, nextLinePrefix + rem);
            doc.cursor.line++;
            doc.cursor.col = static_cast<int>(nextLinePrefix.length());
            doc.modified = true;
        }
    } else if (ch >= 32 && ch <= 126) {
        if (doc.hasSelection) {
            TuiActions::handleDeleteSelection(doc);
        } else if (doc.undoStack.empty() || ch == ' ' || doc.cursor.col == 0) {
            doc.pushUndo();
        }
        doc.lines[doc.cursor.line].insert(doc.cursor.col, 1, static_cast<char>(ch));
        doc.cursor.col++;
        doc.modified = true;
    }
}

} // namespace TuiInput