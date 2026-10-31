#include "tuiactions.h"
#include "tuidialogs.h"

#include <algorithm>
#include <sstream>
#include <cctype>
#include <string>
#include <vector>
#include <set>
#include <cstdio>
#include <regex>

namespace TuiActions {

static std::string s_clipboard;

bool isMarkdownDoc(const EditorDocument &doc) {
    std::string ext = "";
    size_t dot = doc.filePath.find_last_of('.');
    if (dot != std::string::npos) {
        ext = doc.filePath.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    return (ext == "md");
}

static void writeToSystemClipboard(const std::string &text) {
    FILE *fp = popen("wl-copy 2>/dev/null", "w");
    if (!fp) fp = popen("xclip -selection clipboard 2>/dev/null", "w");
    if (!fp) fp = popen("xsel --clipboard --input 2>/dev/null", "w");
    if (fp) {
        fwrite(text.data(), 1, text.size(), fp);
        pclose(fp);
    }
}

static std::string readFromSystemClipboard() {
    std::string out;
    char buffer[512];
    FILE *fp = popen("wl-paste 2>/dev/null", "r");
    if (!fp) fp = popen("xclip -selection clipboard -o 2>/dev/null", "r");
    if (!fp) fp = popen("xsel --clipboard --output 2>/dev/null", "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp)) {
            out += buffer;
        }
        pclose(fp);
    }
    return out.empty() ? s_clipboard : out;
}

static bool isIndexInsideTag(const std::string &line, size_t pos) {
    bool inTag = false;
    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == '<') inTag = true;
        if (i == pos) return inTag;
        if (line[i] == '>') inTag = false;
    }
    return false;
}

static bool rangeOverlapsTag(const std::string &line, size_t pos, size_t len) {
    for (size_t i = pos; i < pos + len && i < line.length(); ++i) {
        if (isIndexInsideTag(line, i) || line[i] == '<' || line[i] == '>') return true;
    }
    return false;
}

static std::vector<std::string> querySystemFonts() {
    std::set<std::string> uniqueFonts;
    FILE *fp = popen("fc-list : family 2>/dev/null", "r");
    if (fp) {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            std::string line(buf);
            std::stringstream ss(line);
            std::string font;
            while (std::getline(ss, font, ',')) {
                size_t start = font.find_first_not_of(" \t\r\n");
                size_t end = font.find_last_not_of(" \t\r\n");
                if (start != std::string::npos && end != std::string::npos) {
                    std::string clean = font.substr(start, end - start + 1);
                    if (!clean.empty() && clean[0] != '.') uniqueFonts.insert(clean);
                }
            }
        }
        pclose(fp);
    }

    if (uniqueFonts.empty()) {
        return {
            "Arial", "Chicago", "Consolas", "Courier New", "DejaVu Sans",
            "DejaVu Serif", "Georgia", "Helvetica", "Liberation Mono",
            "Liberation Sans", "Liberation Serif", "Monospace",
            "Times New Roman", "Trebuchet MS", "Verdana"
        };
    }
    return std::vector<std::string>(uniqueFonts.begin(), uniqueFonts.end());
}

void injectTagAroundSelection(EditorDocument &doc, const std::string &openTag, const std::string &closeTag) {
    doc.pushUndo();

    if (!doc.hasSelection) {
        std::string &cur = doc.lines[doc.cursor.line];
        cur = openTag + cur + closeTag;
        doc.cursor.col = static_cast<int>(cur.length());
        doc.modified = true;
        return;
    }

    TextPosition start = doc.selectionAnchor;
    TextPosition end = doc.cursor;
    if (end < start) std::swap(start, end);

    if (start.line == end.line) {
        std::string &line = doc.lines[start.line];
        std::string selected = line.substr(start.col, end.col - start.col);
        line.replace(start.col, end.col - start.col, openTag + selected + closeTag);
        doc.cursor.col = start.col + static_cast<int>(openTag.length() + selected.length() + closeTag.length());
    } else {
        std::string &firstLine = doc.lines[start.line];
        std::string firstSelected = firstLine.substr(start.col);
        firstLine.replace(start.col, firstSelected.length(), openTag + firstSelected + closeTag);

        for (int i = start.line + 1; i < end.line; ++i) {
            std::string &midLine = doc.lines[i];
            midLine = openTag + midLine + closeTag;
        }

        std::string &lastLine = doc.lines[end.line];
        std::string lastSelected = lastLine.substr(0, end.col);
        lastLine.replace(0, end.col, openTag + lastSelected + closeTag);
        doc.cursor.col = static_cast<int>(openTag.length() + lastSelected.length() + closeTag.length());
    }

    doc.hasSelection = false;
    doc.modified = true;
}

void applyBold(EditorDocument &doc) {
    injectTagAroundSelection(doc, "<b>", "</b>");
}

void applyItalic(EditorDocument &doc) {
    injectTagAroundSelection(doc, "<i>", "</i>");
}

void applyUnderline(EditorDocument &doc) {
    injectTagAroundSelection(doc, "<u>", "</u>");
}

void handleFontSelection(EditorDocument &doc) {
    std::vector<std::string> fonts = querySystemFonts();
    int sel = TuiDialogs::showListSelectionModal("Select Font Family", fonts);
    if (sel >= 0) {
        injectTagAroundSelection(doc, "<span style=\"font-family: '" + fonts[sel] + "';\">", "</span>");
    }
}

void handleFontSizeSelection(EditorDocument &doc) {
    static const std::vector<std::string> sizes = {
        "8pt", "9pt", "10pt", "11pt", "12pt", "14pt", "16pt", "18pt", "20pt", "24pt", "28pt", "32pt", "36pt", "48pt", "72pt"
    };
    int sel = TuiDialogs::showListSelectionModal("Select Font Size", sizes);
    if (sel >= 0) {
        injectTagAroundSelection(doc, "<span style=\"font-size: " + sizes[sel] + ";\">", "</span>");
    }
}

void handleColorSelection(EditorDocument &doc) {
    static const std::vector<std::string> colors = {
        "#000000 (Black)", "#ffffff (White)", "#ff0000 (Red)", "#00aa00 (Green)",
        "#0055ff (Blue)", "#e67e22 (Orange)", "#8e44ad (Purple)", "#16a085 (Teal)",
        "#7f8c8d (Gray)", "#f1c40f (Yellow)"
    };
    static const std::vector<std::string> hexCodes = {
        "#000000", "#ffffff", "#ff0000", "#00aa00",
        "#0055ff", "#e67e22", "#8e44ad", "#16a085",
        "#7f8c8d", "#f1c40f"
    };
    int sel = TuiDialogs::showListSelectionModal("Select Text Color", colors);
    if (sel >= 0) {
        injectTagAroundSelection(doc, "<span style=\"color: " + hexCodes[sel] + ";\">", "</span>");
    }
}

void handleInsertTable(EditorDocument &doc) {
    std::string rowsStr = TuiDialogs::showPromptModal("Table Rows (e.g. 3): ");
    if (rowsStr.empty()) return;
    std::string colsStr = TuiDialogs::showPromptModal("Table Cols (e.g. 3): ");
    if (colsStr.empty()) return;

    int r = std::max(1, atoi(rowsStr.c_str()));
    int c = std::max(1, atoi(colsStr.c_str()));

    doc.pushUndo();

    std::string table = "\n<table border=\"1\">\n";
    for (int i = 0; i < r; ++i) {
        table += "  <tr>";
        for (int j = 0; j < c; ++j) {
            table += "<td>Cell</td>";
        }
        table += "</tr>\n";
    }
    table += "</table>\n";

    auto &curLine = doc.lines[doc.cursor.line];
    curLine.insert(doc.cursor.col, table);
    doc.modified = true;
}

void handleInsertHorizontalRule(EditorDocument &doc) {
    doc.pushUndo();
    std::string tag = "<hr>";

    if (doc.lines[doc.cursor.line].empty()) {
        doc.lines[doc.cursor.line] = tag;
    } else {
        doc.lines.insert(doc.lines.begin() + doc.cursor.line + 1, tag);
        doc.cursor.line++;
    }
    doc.cursor.col = static_cast<int>(tag.length());
    doc.modified = true;
}

void handleInsertBlockquote(EditorDocument &doc) {
    injectTagAroundSelection(doc, "<blockquote>", "</blockquote>");
}

void handleInsertBulletList(EditorDocument &doc) {
    doc.pushUndo();

    int startLine = doc.cursor.line;
    int endLine = doc.cursor.line;
    if (doc.hasSelection) {
        startLine = std::min(doc.selectionAnchor.line, doc.cursor.line);
        endLine = std::max(doc.selectionAnchor.line, doc.cursor.line);
    }

    startLine = std::clamp(startLine, 0, static_cast<int>(doc.lines.size()) - 1);
    endLine = std::clamp(endLine, 0, static_cast<int>(doc.lines.size()) - 1);

    for (int i = startLine; i <= endLine; ++i) {
        std::string &l = doc.lines[i];
        if (l.rfind("• ", 0) == 0) {
            l.erase(0, std::string("• ").length());
        } else if (l.rfind("- ", 0) == 0 || l.rfind("* ", 0) == 0) {
            l.erase(0, 2);
            l.insert(0, "• ");
        } else {
            l.insert(0, "• ");
        }
    }

    doc.cursor.col = static_cast<int>(doc.lines[doc.cursor.line].length());
    doc.hasSelection = false;
    doc.modified = true;
}

void handleInsertNumberedList(EditorDocument &doc) {
    doc.pushUndo();

    int startLine = doc.cursor.line;
    int endLine = doc.cursor.line;
    if (doc.hasSelection) {
        startLine = std::min(doc.selectionAnchor.line, doc.cursor.line);
        endLine = std::max(doc.selectionAnchor.line, doc.cursor.line);
    }

    startLine = std::clamp(startLine, 0, static_cast<int>(doc.lines.size()) - 1);
    endLine = std::clamp(endLine, 0, static_cast<int>(doc.lines.size()) - 1);

    int num = 1;
    for (int i = startLine; i <= endLine; ++i) {
        std::string &l = doc.lines[i];
        size_t dotPos = l.find(". ");
        if (dotPos != std::string::npos && dotPos > 0 && dotPos < 6) {
            bool allDigits = true;
            for (size_t d = 0; d < dotPos; ++d) {
                if (!std::isdigit(static_cast<unsigned char>(l[d]))) {
                    allDigits = false;
                    break;
                }
            }
            if (allDigits) {
                l.erase(0, dotPos + 2);
            }
        } else if (l.rfind("• ", 0) == 0) {
            l.erase(0, std::string("• ").length());
        }

        std::string prefix = std::to_string(num++) + ". ";
        l.insert(0, prefix);
    }

    doc.cursor.col = static_cast<int>(doc.lines[doc.cursor.line].length());
    doc.hasSelection = false;
    doc.modified = true;
}

void handleInsertImage(EditorDocument &doc) {
    std::string imgPath = TuiDialogs::showPromptModal("Enter image file path/name: ");
    if (imgPath.empty()) return;

    // Extract a basic filename for the alt text
    size_t lastSlash = imgPath.find_last_of("/\\");
    std::string altText = (lastSlash == std::string::npos) ? imgPath : imgPath.substr(lastSlash + 1);

    doc.pushUndo();
    std::string tag = "<img src=\"" + imgPath + "\" alt=\"" + altText + "\" />";

    auto &curLine = doc.lines[doc.cursor.line];
    curLine.insert(doc.cursor.col, tag);
    doc.cursor.col += static_cast<int>(tag.length());
    doc.modified = true;
}

void handleDeleteSelection(EditorDocument &doc) {
    if (!doc.hasSelection) return;

    doc.pushUndo();

    TextPosition start = doc.selectionAnchor;
    TextPosition end = doc.cursor;
    if (end < start) std::swap(start, end);

    int totalLines = static_cast<int>(doc.lines.size());
    if (totalLines == 0) {
        doc.lines = {""};
        doc.cursor = {0, 0};
        doc.hasSelection = false;
        return;
    }

    start.line = std::clamp(start.line, 0, totalLines - 1);
    end.line = std::clamp(end.line, 0, totalLines - 1);
    start.col = std::clamp(start.col, 0, static_cast<int>(doc.lines[start.line].length()));
    end.col = std::clamp(end.col, 0, static_cast<int>(doc.lines[end.line].length()));

    if (start.line == end.line) {
        doc.lines[start.line].erase(start.col, end.col - start.col);
    } else {
        std::string merged = doc.lines[start.line].substr(0, start.col) + doc.lines[end.line].substr(end.col);
        doc.lines[start.line] = merged;
        doc.lines.erase(doc.lines.begin() + start.line + 1, doc.lines.begin() + end.line + 1);
    }

    if (doc.lines.empty()) {
        doc.lines.push_back("");
    }

    doc.cursor = start;
    doc.hasSelection = false;
    doc.modified = true;
}

void handleCutCopy(EditorDocument &doc, bool isCut) {
    if (!doc.hasSelection) {
        s_clipboard = doc.lines[doc.cursor.line] + "\n";
        writeToSystemClipboard(s_clipboard);
        if (isCut) {
            doc.pushUndo();
            if (doc.lines.size() > 1) {
                doc.lines.erase(doc.lines.begin() + doc.cursor.line);
                if (doc.cursor.line >= static_cast<int>(doc.lines.size())) {
                    doc.cursor.line = static_cast<int>(doc.lines.size()) - 1;
                }
            } else {
                doc.lines[0].clear();
            }
            doc.cursor.col = 0;
            doc.modified = true;
        }
        return;
    }

    TextPosition start = doc.selectionAnchor;
    TextPosition end = doc.cursor;
    if (end < start) std::swap(start, end);

    if (start.line == end.line) {
        s_clipboard = doc.lines[start.line].substr(start.col, end.col - start.col);
        writeToSystemClipboard(s_clipboard);
        if (isCut) {
            doc.pushUndo();
            doc.lines[start.line].erase(start.col, end.col - start.col);
            doc.cursor = start;
            doc.modified = true;
        }
    } else {
        s_clipboard = doc.lines[start.line].substr(start.col) + "\n";
        for (int i = start.line + 1; i < end.line; ++i) {
            s_clipboard += doc.lines[i] + "\n";
        }
        s_clipboard += doc.lines[end.line].substr(0, end.col);
        writeToSystemClipboard(s_clipboard);

        if (isCut) {
            doc.pushUndo();
            doc.lines[start.line] = doc.lines[start.line].substr(0, start.col) + doc.lines[end.line].substr(end.col);
            doc.lines.erase(doc.lines.begin() + start.line + 1, doc.lines.begin() + end.line + 1);
            doc.cursor = start;
            doc.modified = true;
        }
    }
    doc.hasSelection = false;
}

void handlePaste(EditorDocument &doc) {
    std::string clip = readFromSystemClipboard();
    if (clip.empty()) clip = s_clipboard;
    if (clip.empty()) return;

    doc.pushUndo();

    if (doc.hasSelection) {
        TextPosition start = doc.selectionAnchor;
        TextPosition end = doc.cursor;
        if (end < start) std::swap(start, end);

        int totalLines = static_cast<int>(doc.lines.size());
        if (totalLines > 0) {
            start.line = std::clamp(start.line, 0, totalLines - 1);
            end.line = std::clamp(end.line, 0, totalLines - 1);
            start.col = std::clamp(start.col, 0, static_cast<int>(doc.lines[start.line].length()));
            end.col = std::clamp(end.col, 0, static_cast<int>(doc.lines[end.line].length()));

            if (start.line == end.line) {
                doc.lines[start.line].erase(start.col, end.col - start.col);
            } else {
                std::string merged = doc.lines[start.line].substr(0, start.col) + doc.lines[end.line].substr(end.col);
                doc.lines[start.line] = merged;
                doc.lines.erase(doc.lines.begin() + start.line + 1, doc.lines.begin() + end.line + 1);
            }
        }
        if (doc.lines.empty()) doc.lines.push_back("");
        doc.cursor = start;
        doc.hasSelection = false;
    }

    std::stringstream ss(clip);
    std::string line;
    std::vector<std::string> pasteLines;
    while (std::getline(ss, line)) {
        pasteLines.push_back(line);
    }
    if (pasteLines.empty()) return;

    if (pasteLines.size() == 1) {
        doc.lines[doc.cursor.line].insert(doc.cursor.col, pasteLines[0]);
        doc.cursor.col += static_cast<int>(pasteLines[0].length());
    } else {
        std::string cur = doc.lines[doc.cursor.line];
        std::string prefix = cur.substr(0, doc.cursor.col);
        std::string suffix = cur.substr(doc.cursor.col);

        doc.lines[doc.cursor.line] = prefix + pasteLines[0];
        for (size_t i = 1; i < pasteLines.size(); ++i) {
            doc.lines.insert(doc.lines.begin() + doc.cursor.line + i, pasteLines[i]);
        }
        doc.cursor.line += static_cast<int>(pasteLines.size() - 1);
        doc.lines[doc.cursor.line] += suffix;
        doc.cursor.col = static_cast<int>(pasteLines.back().length());
    }
    doc.modified = true;
}

void handleUndo(EditorDocument &doc) {
    if (doc.undoStack.empty()) return;
    doc.redoStack.push_back({doc.lines, doc.cursor});
    auto snap = doc.undoStack.back();
    doc.undoStack.pop_back();
    doc.lines = snap.lines;
    doc.cursor = snap.cursor;
    doc.modified = true;
}

void handleRedo(EditorDocument &doc) {
    if (doc.redoStack.empty()) return;
    doc.undoStack.push_back({doc.lines, doc.cursor});
    auto snap = doc.redoStack.back();
    doc.redoStack.pop_back();
    doc.lines = snap.lines;
    doc.cursor = snap.cursor;
    doc.modified = true;
}

static bool findNextRegex(EditorDocument &doc, const std::string &findStr, size_t fromLine, size_t fromCol) {
    bool isRegex = true;
    std::regex reg;
    try {
        reg = std::regex(findStr, std::regex::ECMAScript | std::regex::icase);
    } catch (...) {
        isRegex = false;
    }

    for (size_t i = fromLine; i < doc.lines.size(); ++i) {
        size_t startCol = (i == fromLine) ? fromCol : 0;
        const std::string &line = doc.lines[i];
        if (startCol > line.length()) continue;

        if (isRegex) {
            std::smatch m;
            std::string sub = line.substr(startCol);
            if (std::regex_search(sub, m, reg)) {
                size_t matchPos = startCol + m.position(0);
                size_t matchLen = m.length(0);
                if (doc.mode != TuiMode::RichText || !rangeOverlapsTag(line, matchPos, matchLen)) {
                    doc.selectionAnchor = {static_cast<int>(i), static_cast<int>(matchPos)};
                    doc.cursor.line = static_cast<int>(i);
                    doc.cursor.col = static_cast<int>(matchPos + matchLen);
                    doc.hasSelection = true;
                    return true;
                }
            }
        } else {
            size_t pos = line.find(findStr, startCol);
            while (pos != std::string::npos) {
                if (doc.mode != TuiMode::RichText || !rangeOverlapsTag(line, pos, findStr.length())) {
                    doc.selectionAnchor = {static_cast<int>(i), static_cast<int>(pos)};
                    doc.cursor.line = static_cast<int>(i);
                    doc.cursor.col = static_cast<int>(pos + findStr.length());
                    doc.hasSelection = true;
                    return true;
                }
                pos = line.find(findStr, pos + 1);
            }
        }
    }
    return false;
}

static bool replaceNextRegex(EditorDocument &doc, const std::string &findStr, const std::string &replStr, size_t fromLine, size_t fromCol) {
    bool isRegex = true;
    std::regex reg;
    try {
        reg = std::regex(findStr, std::regex::ECMAScript | std::regex::icase);
    } catch (...) {
        isRegex = false;
    }

    for (size_t i = fromLine; i < doc.lines.size(); ++i) {
        size_t startCol = (i == fromLine) ? fromCol : 0;
        const std::string &line = doc.lines[i];
        if (startCol > line.length()) continue;

        if (isRegex) {
            std::smatch m;
            std::string sub = line.substr(startCol);
            if (std::regex_search(sub, m, reg)) {
                size_t matchPos = startCol + m.position(0);
                size_t matchLen = m.length(0);
                if (doc.mode != TuiMode::RichText || !rangeOverlapsTag(line, matchPos, matchLen)) {
                    std::string matchStr = m.str(0);
                    std::string replacedPiece = std::regex_replace(matchStr, reg, replStr);

                    doc.pushUndo();
                    doc.lines[i].replace(matchPos, matchLen, replacedPiece);
                    doc.cursor.line = static_cast<int>(i);
                    doc.cursor.col = static_cast<int>(matchPos + replacedPiece.length());
                    doc.hasSelection = false;
                    doc.modified = true;
                    return true;
                }
            }
        } else {
            size_t pos = line.find(findStr, startCol);
            while (pos != std::string::npos) {
                if (doc.mode != TuiMode::RichText || !rangeOverlapsTag(line, pos, findStr.length())) {
                    doc.pushUndo();
                    doc.lines[i].replace(pos, findStr.length(), replStr);
                    doc.cursor.line = static_cast<int>(i);
                    doc.cursor.col = static_cast<int>(pos + replStr.length());
                    doc.hasSelection = false;
                    doc.modified = true;
                    return true;
                }
                pos = line.find(findStr, pos + 1);
            }
        }
    }
    return false;
}

void handleFindReplace(EditorDocument &doc, const std::string &initFind, const std::string &initReplace) {
    static std::string lastFind;
    static std::string lastReplace;
    static bool lastDoReplace = false;

    std::string findStr = !initFind.empty() ? initFind : lastFind;
    std::string replaceStr = !initReplace.empty() ? initReplace : lastReplace;
    bool doReplace = !initReplace.empty() ? true : lastDoReplace;

    if (!initFind.empty()) {
        doReplace = !initReplace.empty();
    } else {
        if (!TuiDialogs::showFindReplaceModal(findStr, replaceStr, doReplace)) {
            return;
        }
    }

    if (findStr.empty()) return;

    lastFind = findStr;
    lastReplace = replaceStr;
    lastDoReplace = doReplace;

    size_t curLine = static_cast<size_t>(std::max(0, doc.cursor.line));
    size_t curCol = static_cast<size_t>(std::max(0, doc.cursor.col));

    if (doReplace) {
        if (!replaceNextRegex(doc, findStr, replaceStr, curLine, curCol)) {
            replaceNextRegex(doc, findStr, replaceStr, 0, 0);
        }
    } else {
        if (!findNextRegex(doc, findStr, curLine, curCol)) {
            findNextRegex(doc, findStr, 0, 0);
        }
    }
}

void handleIndent(EditorDocument &doc, bool indent) {
    doc.pushUndo();
    int startLine = doc.cursor.line;
    int endLine = doc.cursor.line;
    if (doc.hasSelection) {
        startLine = std::min(doc.selectionAnchor.line, doc.cursor.line);
        endLine = std::max(doc.selectionAnchor.line, doc.cursor.line);
    }

    for (int i = startLine; i <= endLine && i < static_cast<int>(doc.lines.size()); ++i) {
        if (indent) {
            doc.lines[i].insert(0, "    ");
        } else {
            for (int s = 0; s < 4; ++s) {
                if (!doc.lines[i].empty() && doc.lines[i][0] == ' ') {
                    doc.lines[i].erase(0, 1);
                }
            }
        }
    }
    doc.cursor.col = std::min(doc.cursor.col, static_cast<int>(doc.lines[doc.cursor.line].length()));
    doc.modified = true;
}

void handleToggleComment(EditorDocument &doc) {
    doc.pushUndo();
    int startLine = doc.cursor.line;
    int endLine = doc.cursor.line;
    if (doc.hasSelection) {
        startLine = std::min(doc.selectionAnchor.line, doc.cursor.line);
        endLine = std::max(doc.selectionAnchor.line, doc.cursor.line);
    }

    std::string marker = "// ";
    std::string ext = "";
    size_t dot = doc.filePath.find_last_of('.');
    if (dot != std::string::npos) {
        ext = doc.filePath.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    if (ext == "py" || ext == "sh" || ext == "bash" || ext == "ini" || ext == "cfg" || ext == "yaml" || ext == "yml" || ext == "cmake") {
        marker = "# ";
    }
    std::string trimmedMarker = marker;
    trimmedMarker.erase(trimmedMarker.find_last_not_of(" ") + 1);

    bool allCommented = true;
    for (int i = startLine; i <= endLine && i < static_cast<int>(doc.lines.size()); ++i) {
        std::string &l = doc.lines[i];
        size_t first = l.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        if (l.compare(first, trimmedMarker.length(), trimmedMarker) != 0) {
            allCommented = false;
            break;
        }
    }

    for (int i = startLine; i <= endLine && i < static_cast<int>(doc.lines.size()); ++i) {
        std::string &l = doc.lines[i];
        if (allCommented) {
            size_t first = l.find_first_not_of(" \t");
            if (first != std::string::npos && l.compare(first, trimmedMarker.length(), trimmedMarker) == 0) {
                size_t len = (first + marker.length() <= l.length() && l.compare(first, marker.length(), marker) == 0)
                    ? marker.length() : trimmedMarker.length();
                l.erase(first, len);
            }
        } else {
            size_t first = l.find_first_not_of(" \t");
            if (first == std::string::npos) first = 0;
            l.insert(first, marker);
        }
    }
    doc.modified = true;
}

void handleDuplicate(EditorDocument &doc) {
    doc.pushUndo();
    if (doc.hasSelection) {
        TextPosition start = doc.selectionAnchor;
        TextPosition end = doc.cursor;
        if (end < start) std::swap(start, end);

        std::string selText;
        if (start.line == end.line) {
            selText = doc.lines[start.line].substr(start.col, end.col - start.col);
            doc.lines[end.line].insert(end.col, selText);
            doc.cursor.col = end.col + static_cast<int>(selText.length());
        } else {
            selText = doc.lines[start.line].substr(start.col) + "\n";
            for (int i = start.line + 1; i < end.line; ++i) {
                selText += doc.lines[i] + "\n";
            }
            selText += doc.lines[end.line].substr(0, end.col);

            std::stringstream ss(selText);
            std::string line;
            std::vector<std::string> dupLines;
            while (std::getline(ss, line)) dupLines.push_back(line);

            std::string cur = doc.lines[end.line];
            std::string prefix = cur.substr(0, end.col);
            std::string suffix = cur.substr(end.col);

            doc.lines[end.line] = prefix + dupLines[0];
            for (size_t i = 1; i < dupLines.size(); ++i) {
                doc.lines.insert(doc.lines.begin() + end.line + i, dupLines[i]);
            }
            doc.cursor.line = end.line + static_cast<int>(dupLines.size() - 1);
            doc.lines[doc.cursor.line] += suffix;
            doc.cursor.col = static_cast<int>(dupLines.back().length());
        }
    } else {
        std::string text = doc.lines[doc.cursor.line];
        doc.lines.insert(doc.lines.begin() + doc.cursor.line + 1, text);
        doc.cursor.line++;
    }
    doc.hasSelection = false;
    doc.modified = true;
}

void handleMoveLine(EditorDocument &doc, bool moveUp) {
    int startLine = doc.cursor.line;
    int endLine = doc.cursor.line;
    if (doc.hasSelection) {
        startLine = std::min(doc.selectionAnchor.line, doc.cursor.line);
        endLine = std::max(doc.selectionAnchor.line, doc.cursor.line);
    }

    if (moveUp) {
        if (startLine == 0) return;
        doc.pushUndo();
        std::string prev = doc.lines[startLine - 1];
        doc.lines.erase(doc.lines.begin() + startLine - 1);
        doc.lines.insert(doc.lines.begin() + endLine, prev);
        doc.cursor.line--;
        if (doc.hasSelection) doc.selectionAnchor.line--;
    } else {
        if (endLine + 1 >= static_cast<int>(doc.lines.size())) return;
        doc.pushUndo();
        std::string next = doc.lines[endLine + 1];
        doc.lines.erase(doc.lines.begin() + endLine + 1);
        doc.lines.insert(doc.lines.begin() + startLine, next);
        doc.cursor.line++;
        if (doc.hasSelection) doc.selectionAnchor.line++;
    }
    doc.modified = true;
}

bool handleSmartBackspace(EditorDocument &doc) {
    if (doc.hasSelection || doc.cursor.col == 0) return false;
    const std::string &line = doc.lines[doc.cursor.line];
    bool allSpaces = true;
    for (int i = 0; i < doc.cursor.col; ++i) {
        if (line[i] != ' ') { allSpaces = false; break; }
    }
    if (allSpaces && doc.cursor.col >= 2) {
        doc.pushUndo();
        int spacesToDelete = doc.cursor.col % 4;
        if (spacesToDelete == 0) spacesToDelete = 4;
        spacesToDelete = std::min(spacesToDelete, doc.cursor.col);
        doc.lines[doc.cursor.line].erase(doc.cursor.col - spacesToDelete, spacesToDelete);
        doc.cursor.col -= spacesToDelete;
        doc.modified = true;
        return true;
    }
    return false;
}

void handleSmartReturn(EditorDocument &doc) {
    doc.pushUndo();
    std::string cur = doc.lines[doc.cursor.line];
    std::string rem = cur.substr(doc.cursor.col);
    std::string prefix = cur.substr(0, doc.cursor.col);

    size_t indentLen = 0;
    while (indentLen < prefix.length() && (prefix[indentLen] == ' ' || prefix[indentLen] == '\t')) {
        indentLen++;
    }
    std::string indent = prefix.substr(0, indentLen);

    size_t lastNonSpace = prefix.find_last_not_of(" \t");
    if (lastNonSpace != std::string::npos) {
        char endChar = prefix[lastNonSpace];
        if (endChar == '{' || endChar == '(' || endChar == '[' || endChar == ':') {
            indent += "    ";
        }
    }

    doc.lines[doc.cursor.line] = prefix;
    doc.lines.insert(doc.lines.begin() + doc.cursor.line + 1, indent + rem);
    doc.cursor.line++;
    doc.cursor.col = static_cast<int>(indent.length());
    doc.modified = true;
}

void handleStripBlankLines(EditorDocument &doc) {
    doc.pushUndo();
    std::vector<std::string> filtered;
    for (const auto &l : doc.lines) {
        bool hasChar = false;
        for (char ch : l) {
            if (!std::isspace(static_cast<unsigned char>(ch))) {
                hasChar = true;
                break;
            }
        }
        if (hasChar) filtered.push_back(l);
    }
    if (filtered.empty()) filtered.push_back("");
    doc.lines = filtered;
    doc.cursor.line = std::min(doc.cursor.line, static_cast<int>(doc.lines.size()) - 1);
    doc.cursor.col = std::min(doc.cursor.col, static_cast<int>(doc.lines[doc.cursor.line].length()));
    doc.modified = true;
}

void handleStripTrailingSpaces(EditorDocument &doc) {
    doc.pushUndo();
    bool changed = false;

    for (auto &l : doc.lines) {
        size_t endPos = l.find_last_not_of(" \t");
        if (endPos == std::string::npos) {
            if (!l.empty()) {
                l.clear();
                changed = true;
            }
        } else if (endPos + 1 < l.length()) {
            l.erase(endPos + 1);
            changed = true;
        }
    }

    if (changed) {
        doc.cursor.col = std::min(doc.cursor.col, static_cast<int>(doc.lines[doc.cursor.line].length()));
        doc.modified = true;
    }
}

void handleSortLines(EditorDocument &doc) {
    doc.pushUndo();
    int startLine = 0;
    int endLine = static_cast<int>(doc.lines.size()) - 1;
    if (doc.hasSelection) {
        startLine = std::min(doc.selectionAnchor.line, doc.cursor.line);
        endLine = std::max(doc.selectionAnchor.line, doc.cursor.line);
    }
    std::sort(doc.lines.begin() + startLine, doc.lines.begin() + endLine + 1, [](const std::string &a, const std::string &b) {
        std::string la = a, lb = b;
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        return la < lb;
    });
    doc.modified = true;
}

void handleReverseLines(EditorDocument &doc) {
    doc.pushUndo();
    int startLine = 0;
    int endLine = static_cast<int>(doc.lines.size()) - 1;
    if (doc.hasSelection) {
        startLine = std::min(doc.selectionAnchor.line, doc.cursor.line);
        endLine = std::max(doc.selectionAnchor.line, doc.cursor.line);
    }
    std::reverse(doc.lines.begin() + startLine, doc.lines.begin() + endLine + 1);
    doc.modified = true;
}

void handleChangeCase(EditorDocument &doc, bool upper) {
    doc.pushUndo();
    int startLine = doc.cursor.line;
    int endLine = doc.cursor.line;
    if (doc.hasSelection) {
        startLine = std::min(doc.selectionAnchor.line, doc.cursor.line);
        endLine = std::max(doc.selectionAnchor.line, doc.cursor.line);
    }

    for (int i = startLine; i <= endLine && i < static_cast<int>(doc.lines.size()); ++i) {
        if (doc.mode == TuiMode::RichText) {
            bool inTag = false;
            for (char &c : doc.lines[i]) {
                if (c == '<') inTag = true;
                else if (c == '>') inTag = false;
                else if (!inTag) {
                    c = upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                              : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
            }
        } else {
            for (char &c : doc.lines[i]) {
                c = upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                          : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
    }
    doc.modified = true;
}

void handleSpellCheck(EditorDocument &doc) {
    std::string word;
    TextPosition wordStart, wordEnd;

    if (doc.hasSelection) {
        wordStart = doc.selectionAnchor;
        wordEnd = doc.cursor;
        if (wordEnd < wordStart) std::swap(wordStart, wordEnd);
        if (wordStart.line == wordEnd.line) {
            word = doc.lines[wordStart.line].substr(wordStart.col, wordEnd.col - wordStart.col);
        }
    } else {
        const std::string &line = doc.lines[doc.cursor.line];
        if (line.empty()) return;
        int col = std::min(doc.cursor.col, static_cast<int>(line.length()) - 1);
        if (col < 0) col = 0;
        int start = col, end = col;
        while (start > 0 && (std::isalnum(static_cast<unsigned char>(line[start - 1])) || line[start - 1] == '\'')) {
            start--;
        }
        while (end < static_cast<int>(line.length()) && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '\'')) {
            end++;
        }
        if (start < end) {
            word = line.substr(start, end - start);
            wordStart = {doc.cursor.line, start};
            wordEnd = {doc.cursor.line, end};
        }
    }

    if (word.empty()) return;

    std::string cleanWord;
    for (char c : word) {
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '\'') {
            cleanWord.push_back(c);
        }
    }
    if (cleanWord.empty()) return;

    std::string cmd = "echo \"" + cleanWord + "\" | aspell -a 2>/dev/null";
    FILE *fp = popen(cmd.c_str(), "r");
    std::vector<std::string> suggestions;
    if (fp) {
        char buf[1024];
        while (fgets(buf, sizeof(buf), fp)) {
            std::string l(buf);
            if (!l.empty() && l.back() == '\n') l.pop_back();
            if (!l.empty() && l.back() == '\r') l.pop_back();
            if (l.rfind("& ", 0) == 0) {
                size_t colon = l.find(':');
                if (colon != std::string::npos) {
                    std::string rest = l.substr(colon + 2);
                    std::stringstream ss(rest);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        size_t first = item.find_first_not_of(" ");
                        size_t last = item.find_last_not_of(" ");
                        if (first != std::string::npos && last != std::string::npos) {
                            suggestions.push_back(item.substr(first, last - first + 1));
                        }
                    }
                }
            }
        }
        pclose(fp);
    }

    int choice = TuiDialogs::showSpellSuggestionsModal(cleanWord, suggestions);
    if (choice >= 0 && choice < static_cast<int>(suggestions.size())) {
        doc.pushUndo();
        doc.lines[wordStart.line].replace(wordStart.col, wordEnd.col - wordStart.col, suggestions[choice]);
        doc.cursor.line = wordStart.line;
        doc.cursor.col = wordStart.col + static_cast<int>(suggestions[choice].length());
        doc.hasSelection = false;
        doc.modified = true;
    }
}

} // namespace TuiActions