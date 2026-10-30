#include "ncurses.h"
#include "tuitypes.h"
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

static std::string formatInlineMarkdownToHtml(std::string text) {
    text = std::regex_replace(text, std::regex(R"(`([^`]+)`)"), "<code>$1</code>");
    text = std::regex_replace(text, std::regex(R"(\*\*([^*]+)\*\*)"), "<strong>$1</strong>");
    text = std::regex_replace(text, std::regex(R"(__([^_]+)__)"), "<strong>$1</strong>");
    text = std::regex_replace(text, std::regex(R"(\*([^*]+)\*)"), "<em>$1</em>");
    text = std::regex_replace(text, std::regex(R"(_([^_]+)_)"), "<em>$1</em>");
    return text;
}

static std::string formatInlineHtmlToMarkdown(std::string text) {
    text = std::regex_replace(text, std::regex(R"(<code[^>]*>(.*?)</code>)"), "`$1`");
    text = std::regex_replace(text, std::regex(R"(<(strong|b)[^>]*>(.*?)</\1>)"), "**$2**");
    text = std::regex_replace(text, std::regex(R"(<(em|i)[^>]*>(.*?)</\1>)"), "*$2*");
    text = std::regex_replace(text, std::regex(R"(<span[^>]*font-weight:\s*(700|bold)[^>]*>(.*?)</span>)"), "**$2**");
    text = std::regex_replace(text, std::regex(R"(<span[^>]*>(.*?)</span>)"), "$1");
    return text;
}

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

        while (m_running) {
            updateScroll();
            TuiView::render(m_documents, m_activeDocIdx, false, 0, m_topMenus, m_saveFlashActive);
            if (m_saveFlashActive) {
                m_saveFlashActive = false;
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

    std::string serializeLinesToMarkdown(const std::vector<std::string> &lines) {
        std::ostringstream out;
        size_t i = 0;

        while (i < lines.size()) {
            std::string l = lines[i];

            if (l.find("<table") != std::string::npos) {
                std::string tableContent;
                while (i < lines.size()) {
                    tableContent += lines[i] + "\n";
                    if (lines[i].find("</table>") != std::string::npos) break;
                    ++i;
                }

                std::vector<std::vector<std::string>> rows;
                std::regex trRegex(R"(<tr[^>]*>(.*?)</tr>)");
                std::regex cellRegex(R"(<(td|th)[^>]*>(.*?)</\1>)");

                auto trBegin = std::sregex_iterator(tableContent.begin(), tableContent.end(), trRegex);
                auto trEnd = std::sregex_iterator();
                for (auto tri = trBegin; tri != trEnd; ++tri) {
                    std::string rowHtml = (*tri)[1].str();
                    std::vector<std::string> cells;
                    auto cellBegin = std::sregex_iterator(rowHtml.begin(), rowHtml.end(), cellRegex);
                    auto cellEnd = std::sregex_iterator();
                    for (auto ci = cellBegin; ci != cellEnd; ++ci) {
                        std::string cText = (*ci)[2].str();
                        cText = std::regex_replace(cText, std::regex(R"(</?p[^>]*>)"), "");
                        cells.push_back(formatInlineHtmlToMarkdown(cText));
                    }
                    if (!cells.empty()) rows.push_back(cells);
                }

                if (!rows.empty()) {
                    size_t maxCols = 0;
                    for (const auto &r : rows) maxCols = std::max(maxCols, r.size());

                    out << "\n|";
                    for (size_t c = 0; c < maxCols; ++c) {
                        std::string val = (c < rows[0].size()) ? rows[0][c] : " ";
                        out << " " << val << " |";
                    }
                    out << "\n|";
                    for (size_t c = 0; c < maxCols; ++c) {
                        out << " --- |";
                    }
                    out << "\n";
                    for (size_t r = 1; r < rows.size(); ++r) {
                        out << "|";
                        for (size_t c = 0; c < maxCols; ++c) {
                            std::string val = (c < rows[r].size()) ? rows[r][c] : " ";
                            out << " " << val << " |";
                        }
                        out << "\n";
                    }
                    out << "\n";
                }
                ++i;
                continue;
            }

            std::regex hRegex(R"(<h([1-6])[^>]*>(.*?)</h\1>)");
            std::smatch hMatch;
            if (std::regex_search(l, hMatch, hRegex)) {
                int level = std::stoi(hMatch[1].str());
                std::string content = hMatch[2].str();
                std::string prefix(level, '#');
                out << prefix << " " << formatInlineHtmlToMarkdown(content) << "\n\n";
                ++i;
                continue;
            }

            std::regex liRegex(R"(<li[^>]*>(.*?)</li>)");
            std::smatch liMatch;
            if (std::regex_search(l, liMatch, liRegex)) {
                std::string content = liMatch[1].str();
                out << "- " << formatInlineHtmlToMarkdown(content) << "\n";
                ++i;
                continue;
            }

            if (l.find("<hr") != std::string::npos) {
                out << "---\n\n";
                ++i;
                continue;
            }

            if (l.find("<blockquote") != std::string::npos) {
                std::string q = std::regex_replace(l, std::regex(R"(</?blockquote[^>]*>)"), "");
                size_t first = q.find_first_not_of(" \t");
                q = (first != std::string::npos) ? ("> " + q.substr(first)) : "> ";
                out << formatInlineHtmlToMarkdown(q) << "\n\n";
                ++i;
                continue;
            }

            std::regex pRegex(R"(<p[^>]*>(.*?)</p>)");
            std::smatch pMatch;
            if (std::regex_search(l, pMatch, pRegex)) {
                std::string content = pMatch[1].str();
                if (content == "<br />" || content == "<br>" || content == "<br/>" || content.empty()) {
                    out << "\n";
                } else {
                    out << formatInlineHtmlToMarkdown(content) << "\n\n";
                }
                ++i;
                continue;
            }

            if (l.find("<!DOCTYPE") != std::string::npos || l.find("<html") != std::string::npos ||
                l.find("</html>") != std::string::npos || l.find("<head") != std::string::npos ||
                l.find("</head>") != std::string::npos || l.find("<body") != std::string::npos ||
                l.find("</body>") != std::string::npos || l.find("<style") != std::string::npos ||
                l.find("</style>") != std::string::npos || l.find("<ul>") != std::string::npos ||
                l.find("</ul>") != std::string::npos || l.find("<ol>") != std::string::npos ||
                l.find("</ol>") != std::string::npos) {
                ++i;
                continue;
            }

            if (l.rfind("• ", 0) == 0) {
                l = "- " + l.substr(std::string("• ").length());
            }

            out << formatInlineHtmlToMarkdown(l) << "\n";
            ++i;
        }

        std::string res = out.str();
        res = std::regex_replace(res, std::regex(R"(\n{3,})"), "\n\n");
        return res;
    }

    std::string serializeLinesToHtml(const std::vector<std::string> &lines) {
        bool hasHtmlRoot = false;
        for (const auto &l : lines) {
            if (l.find("<html") != std::string::npos || l.find("<!DOCTYPE") != std::string::npos) {
                hasHtmlRoot = true;
                break;
            }
        }
        if (hasHtmlRoot) {
            std::ostringstream out;
            for (size_t i = 0; i < lines.size(); ++i) {
                out << lines[i];
                if (i + 1 < lines.size()) out << "\n";
            }
            return out.str();
        }

        std::ostringstream out;
        out << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\" />\n";
        out << "<style type=\"text/css\">\n";
        out << "p { margin: 0px; white-space: pre-wrap; }\n";
        out << "ul, ol { margin: 0px 0px 0px 24px; padding: 0px; }\n";
        out << "table { border-collapse: collapse; margin: 10px 0; }\n";
        out << "th, td { border: 1px solid #777; padding: 4px 8px; }\n";
        out << "</style></head><body>\n";

        bool inUl = false;
        bool inOl = false;
        size_t i = 0;

        while (i < lines.size()) {
            std::string l = lines[i];

            if (l.rfind("|", 0) == 0 && l.length() > 1 && l.back() == '|') {
                if (inUl) { out << "</ul>\n"; inUl = false; }
                if (inOl) { out << "</ol>\n"; inOl = false; }

                std::vector<std::vector<std::string>> tableRows;
                while (i < lines.size() && lines[i].rfind("|", 0) == 0 && lines[i].length() > 1 && lines[i].back() == '|') {
                    std::string rowLine = lines[i];
                    bool isSep = true;
                    for (char c : rowLine) {
                        if (c != '|' && c != '-' && c != ':' && c != ' ' && c != '\t') {
                            isSep = false;
                            break;
                        }
                    }
                    if (!isSep) {
                        std::vector<std::string> cells;
                        size_t pos = 1;
                        while (pos < rowLine.length()) {
                            size_t nextPipe = rowLine.find('|', pos);
                            if (nextPipe == std::string::npos) break;
                            std::string cell = rowLine.substr(pos, nextPipe - pos);
                            size_t start = cell.find_first_not_of(" \t");
                            size_t end = cell.find_last_not_of(" \t");
                            cells.push_back((start != std::string::npos) ? cell.substr(start, end - start + 1) : "");
                            pos = nextPipe + 1;
                        }
                        if (!cells.empty()) tableRows.push_back(cells);
                    }
                    ++i;
                }

                if (!tableRows.empty()) {
                    out << "<table>\n";
                    for (size_t r = 0; r < tableRows.size(); ++r) {
                        out << "<tr>";
                        for (const auto &cell : tableRows[r]) {
                            std::string tag = (r == 0) ? "th" : "td";
                            out << "<" << tag << ">" << formatInlineMarkdownToHtml(cell) << "</" << tag << ">";
                        }
                        out << "</tr>\n";
                    }
                    out << "</table>\n";
                }
                continue;
            }

            std::string trimmed = l;
            size_t tStart = trimmed.find_first_not_of(" \t");
            size_t tEnd = trimmed.find_last_not_of(" \t");
            trimmed = (tStart != std::string::npos) ? trimmed.substr(tStart, tEnd - tStart + 1) : "";

            if (trimmed == "---" || trimmed == "***" || trimmed == "___" || trimmed == "<hr>" || trimmed == "<hr />" || trimmed == "<hr/>") {
                if (inUl) { out << "</ul>\n"; inUl = false; }
                if (inOl) { out << "</ol>\n"; inOl = false; }
                out << "<hr />\n";
                ++i;
                continue;
            }

            if (trimmed.rfind("#", 0) == 0) {
                size_t hCount = 0;
                while (hCount < trimmed.length() && trimmed[hCount] == '#') hCount++;
                if (hCount <= 6 && hCount < trimmed.length() && (trimmed[hCount] == ' ' || trimmed[hCount] == '\t')) {
                    if (inUl) { out << "</ul>\n"; inUl = false; }
                    if (inOl) { out << "</ol>\n"; inOl = false; }
                    size_t cStart = trimmed.find_first_not_of(" \t", hCount);
                    std::string hText = (cStart != std::string::npos) ? trimmed.substr(cStart) : "";
                    out << "<h" << hCount << ">" << formatInlineMarkdownToHtml(hText) << "</h" << hCount << ">\n";
                    ++i;
                    continue;
                }
            }

            if (trimmed.rfind(">", 0) == 0) {
                if (inUl) { out << "</ul>\n"; inUl = false; }
                if (inOl) { out << "</ol>\n"; inOl = false; }
                size_t bStart = trimmed.find_first_not_of(" \t", 1);
                std::string qText = (bStart != std::string::npos) ? trimmed.substr(bStart) : "";
                out << "<blockquote>" << formatInlineMarkdownToHtml(qText) << "</blockquote>\n";
                ++i;
                continue;
            }

            bool isBullet = (trimmed.rfind("• ", 0) == 0 || trimmed.rfind("- ", 0) == 0 || trimmed.rfind("* ", 0) == 0);
            std::string bulletText = "";
            if (trimmed.rfind("• ", 0) == 0) bulletText = trimmed.substr(std::string("• ").length());
            else if (trimmed.rfind("- ", 0) == 0 || trimmed.rfind("* ", 0) == 0) bulletText = trimmed.substr(2);

            size_t dotPos = trimmed.find(". ");
            bool isNumbered = false;
            if (dotPos != std::string::npos && dotPos > 0 && dotPos < 6) {
                bool allDigits = true;
                for (size_t di = 0; di < dotPos; ++di) {
                    if (!std::isdigit(static_cast<unsigned char>(trimmed[di]))) {
                        allDigits = false;
                        break;
                    }
                }
                if (allDigits) isNumbered = true;
            }

            if (isBullet) {
                if (inOl) { out << "</ol>\n"; inOl = false; }
                if (!inUl) { out << "<ul>\n"; inUl = true; }
                out << "<li>" << formatInlineMarkdownToHtml(bulletText) << "</li>\n";
            } else if (isNumbered) {
                if (inUl) { out << "</ul>\n"; inUl = false; }
                if (!inOl) { out << "<ol>\n"; inOl = true; }
                out << "<li>" << formatInlineMarkdownToHtml(trimmed.substr(dotPos + 2)) << "</li>\n";
            } else {
                if (inUl) { out << "</ul>\n"; inUl = false; }
                if (inOl) { out << "</ol>\n"; inOl = false; }

                if (trimmed.empty()) {
                    out << "<p><br /></p>\n";
                } else if (trimmed.find("<table") != std::string::npos || trimmed.find("<tr") != std::string::npos || trimmed.find("<td") != std::string::npos) {
                    out << l << "\n";
                } else {
                    out << "<p>" << formatInlineMarkdownToHtml(l) << "</p>\n";
                }
            }
            ++i;
        }

        if (inUl) out << "</ul>\n";
        if (inOl) out << "</ol>\n";

        out << "</body></html>\n";
        return out.str();
    }

    std::string serializeLinesToPlainText(const std::vector<std::string> &lines) {
        std::ostringstream out;
        for (size_t i = 0; i < lines.size(); ++i) {
            std::string l = lines[i];
            if (l.rfind("• ", 0) == 0) {
                l = "- " + l.substr(std::string("• ").length());
            } else if (l.find("<hr") != std::string::npos) {
                l = std::regex_replace(l, std::regex("<hr\\s*/?>"), "----------------------------------------");
            } else if (l.find("<blockquote") != std::string::npos) {
                l = std::regex_replace(l, std::regex("</?blockquote>"), "");
                size_t first = l.find_first_not_of(" \t");
                l = (first != std::string::npos) ? ("> " + l.substr(first)) : "> ";
            }
            l = std::regex_replace(l, std::regex("<[^>]*>"), "");
            out << l;
            if (i + 1 < lines.size()) out << "\n";
        }
        return out.str();
    }

    std::string serializeDocToRaw(const EditorDocument &d) {
        std::string ext = "";
        size_t dot = d.filePath.find_last_of('.');
        if (dot != std::string::npos) {
            ext = d.filePath.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        if (ext == "md") {
            return serializeLinesToMarkdown(d.lines);
        }

        if (ext == "txt") {
            return serializeLinesToPlainText(d.lines);
        }

        bool isHtml = (ext == "html" || ext == "htm" || d.filePath.empty());
        if (isHtml) {
            return serializeLinesToHtml(d.lines);
        }

        std::ostringstream out;
        for (size_t i = 0; i < d.lines.size(); ++i) {
            out << d.lines[i];
            if (i + 1 < d.lines.size()) out << "\n";
        }
        return out.str();
    }

    void deserializeRawToRich(EditorDocument &d, const std::string &rawContent) {
        std::string ext = "";
        size_t dot = d.filePath.find_last_of('.');
        if (dot != std::string::npos) {
            ext = d.filePath.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        d.lines.clear();

        bool isHtml = (ext == "html" || ext == "htm" || d.filePath.empty());
        if (isHtml && (rawContent.find("<p") != std::string::npos || rawContent.find("<li") != std::string::npos)) {
            size_t pos = 0;
            int currentOlNum = 1;
            bool insideOl = false;

            while (pos < rawContent.length()) {
                size_t nextTag = rawContent.find('<', pos);
                if (nextTag == std::string::npos) break;

                if (rawContent.compare(nextTag, 4, "<ol>", 4) == 0 || rawContent.compare(nextTag, 4, "<ol ", 4) == 0) {
                    insideOl = true;
                    currentOlNum = 1;
                    pos = rawContent.find('>', nextTag) + 1;
                    continue;
                }
                if (rawContent.compare(nextTag, 5, "</ol>", 5) == 0) {
                    insideOl = false;
                    pos = nextTag + 5;
                    continue;
                }
                if (rawContent.compare(nextTag, 4, "<ul>", 4) == 0 || rawContent.compare(nextTag, 4, "<ul ", 4) == 0) {
                    insideOl = false;
                    pos = rawContent.find('>', nextTag) + 1;
                    continue;
                }
                if (rawContent.compare(nextTag, 5, "</ul>", 5) == 0) {
                    pos = nextTag + 5;
                    continue;
                }

                if (rawContent.compare(nextTag, 2, "<p", 2) == 0 && (rawContent[nextTag + 2] == '>' || rawContent[nextTag + 2] == ' ')) {
                    size_t closeTag = rawContent.find('>', nextTag);
                    if (closeTag == std::string::npos) break;
                    size_t endP = rawContent.find("</p>", closeTag);
                    if (endP == std::string::npos) break;

                    std::string lineContent = rawContent.substr(closeTag + 1, endP - closeTag - 1);
                    if (lineContent == "<br />" || lineContent == "<br>" || lineContent == "<br/>") {
                        d.lines.push_back("");
                    } else {
                        d.lines.push_back(lineContent);
                    }
                    pos = endP + 4;
                    continue;
                }

                if (rawContent.compare(nextTag, 3, "<li", 3) == 0 && (rawContent[nextTag + 3] == '>' || rawContent[nextTag + 3] == ' ')) {
                    size_t closeTag = rawContent.find('>', nextTag);
                    if (closeTag == std::string::npos) break;
                    size_t endLi = rawContent.find("</li>", closeTag);
                    if (endLi == std::string::npos) break;

                    std::string liContent = rawContent.substr(closeTag + 1, endLi - closeTag - 1);
                    if (insideOl) {
                        d.lines.push_back(std::to_string(currentOlNum++) + ". " + liContent);
                    } else {
                        d.lines.push_back("• " + liContent);
                    }
                    pos = endLi + 5;
                    continue;
                }

                pos = nextTag + 1;
            }
        } else {
            std::stringstream ss(rawContent);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (ext == "md" || ext == "txt") {
                    if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) {
                        line = "• " + line.substr(2);
                    } else if (line == "---" || line == "***" || line == "___") {
                        line = "<hr>";
                    } else if (line.rfind("> ", 0) == 0) {
                        line = "<blockquote>" + line.substr(2) + "</blockquote>";
                    } else if (line == ">") {
                        line = "<blockquote></blockquote>";
                    }
                }
                d.lines.push_back(line);
            }
        }
        if (d.lines.empty()) d.lines.push_back("");
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

        if (targetMode == TuiMode::Code) {
            std::string raw = serializeDocToRaw(doc());
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
            deserializeRawToRich(doc(), ss.str());
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
                deserializeRawToRich(d, rawContent);
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

        bool bufferHasHtml = false;
        for (const auto &l : doc().lines) {
            if (l.find("<html") != std::string::npos || l.find("<!DOCTYPE") != std::string::npos ||
                l.find("<p") != std::string::npos || l.find("<div") != std::string::npos ||
                l.find("<table") != std::string::npos) {
                bufferHasHtml = true;
                break;
            }
        }

        std::string finalOutput;

        if (savingAsHtml) {
            if (bufferHasHtml) {
                std::ostringstream ss;
                for (size_t i = 0; i < doc().lines.size(); ++i) {
                    ss << doc().lines[i];
                    if (i + 1 < doc().lines.size()) ss << "\n";
                }
                finalOutput = ss.str();
            } else {
                finalOutput = serializeLinesToHtml(doc().lines);
            }
        } else if (savingAsMd) {
            if (bufferHasHtml) {
                finalOutput = serializeLinesToMarkdown(doc().lines);
            } else {
                std::ostringstream ss;
                for (size_t i = 0; i < doc().lines.size(); ++i) {
                    ss << doc().lines[i];
                    if (i + 1 < doc().lines.size()) ss << "\n";
                }
                finalOutput = ss.str();
            }
        } else if (savingAsTxt) {
            finalOutput = serializeLinesToPlainText(doc().lines);
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

    void cutEntireLine() {
        doc().pushUndo();
        TuiActions::handleCutCopy(doc(), true);
        doc().modified = true;
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

    void handleMouseEvent(const MEVENT &event) {
#if defined(NCURSES_MOUSE_VERSION) && (NCURSES_MOUSE_VERSION > 1)
        const mmask_t wheelUp = BUTTON4_PRESSED;
        const mmask_t wheelDown = BUTTON5_PRESSED;
#else
        const mmask_t wheelUp = 0x10000;
        const mmask_t wheelDown = 0x200000;
#endif

        if (event.bstate & wheelUp) {
            int scrollStep = 3;
            doc().scrollLine = std::max(0, doc().scrollLine - scrollStep);
            int contentHeight = std::max(1, LINES - 4);
            if (doc().cursor.line > doc().scrollLine + contentHeight - 1) {
                doc().cursor.line = doc().scrollLine + contentHeight - 1;
                doc().cursor.col = std::min(doc().cursor.col, static_cast<int>(doc().lines[doc().cursor.line].length()));
            }
            return;
        }
        if (event.bstate & wheelDown) {
            int maxScroll = std::max(0, static_cast<int>(doc().lines.size()) - 1);
            int scrollStep = 3;
            doc().scrollLine = std::min(maxScroll, doc().scrollLine + scrollStep);
            if (doc().cursor.line < doc().scrollLine) {
                doc().cursor.line = doc().scrollLine;
                doc().cursor.col = std::min(doc().cursor.col, static_cast<int>(doc().lines[doc().cursor.line].length()));
            }
            return;
        }

        int my = event.y, mx = event.x;

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

            if ((event.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) && clickedTab >= 0) {
                int res = TuiDialogs::showTabContextMenuModal(m_documents[clickedTab].title);
                if (res == 1) {
                    size_t oldActive = m_activeDocIdx;
                    m_activeDocIdx = clickedTab;
                    saveCurrentFile();
                    m_activeDocIdx = oldActive;
                } else if (res == 2) {
                    closeDocument(clickedTab);
                } else if (res == 3) {
                    size_t oldActive = m_activeDocIdx;
                    m_activeDocIdx = clickedTab;
                    saveCurrentFile();
                    m_activeDocIdx = oldActive;
                    closeDocument(clickedTab);
                }
                return;
            }

            if (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClickTime).count();

                if (clickedTab >= 0) {
                    if (clickedTab == m_lastClickTabIdx && elapsed < 350) {
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

        if (my == 2 && (event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
            int toolOffset = 1;
            if (doc().mode == TuiMode::RichText) {
                for (size_t b = 0; b < TuiView::toolbarButtons.size(); ++b) {
                    int btnLen = static_cast<int>(TuiView::toolbarButtons[b].length()) + 4;
                    if (mx >= toolOffset && mx < toolOffset + btnLen) {
                        if (b == 0) saveCurrentFile();
                        else if (b == 1) promptOpen();
                        else if (b == 2) TuiActions::handleFontSelection(doc());
                        else if (b == 3) TuiActions::handleFontSizeSelection(doc());
                        else if (b == 4) TuiActions::injectTagAroundSelection(doc(), "<b>", "</b>");
                        else if (b == 5) TuiActions::injectTagAroundSelection(doc(), "<i>", "</i>");
                        return;
                    }
                    toolOffset += btnLen + 1;
                }
            } else {
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

        int contentTop = 3;
        int contentBottom = LINES - 2;
        if ((my >= contentTop && my <= contentBottom) || m_isDragging) {
            int lineNumWidth = (doc().mode == TuiMode::Code) ? 6 : 0;
            int clampedY = std::clamp(my, contentTop, contentBottom);
            int targetLine = doc().scrollLine + (clampedY - contentTop);
            targetLine = std::clamp(targetLine, 0, static_cast<int>(doc().lines.size()) - 1);

            if (doc().mode == TuiMode::Code && mx < lineNumWidth) {
                doc().selectionAnchor = {targetLine, 0};
                doc().cursor.line = targetLine;
                doc().cursor.col = static_cast<int>(doc().lines[targetLine].length());
                doc().hasSelection = true;
                return;
            }

            int targetCol = std::max(0, mx - lineNumWidth);
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
            return;
        }
    }

    void handleInput(int ch) {
        if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK) handleMouseEvent(ev);
            return;
        }

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
        if (ch == KEY_F(4) || ch == 6) {
            TuiActions::handleFindReplace(doc());
            return;
        }
        if (ch == KEY_F(5)) { switchDocumentMode(TuiMode::RichText); return; }
        if (ch == KEY_F(6)) { switchDocumentMode(TuiMode::Code); return; }
        if (ch == KEY_F(7) || ch == 15) {
            promptOpen();
            return;
        }
        if (ch == KEY_F(8) || ch == 19) {
            saveCurrentFile();
            return;
        }
        if (ch == KEY_F(9)) {
            TuiActions::handleSpellCheck(doc());
            return;
        }
        if (ch == KEY_F(10)) { exportDocument("ODF", "odt"); return; }
        if (ch == KEY_F(11)) {
            int sel = 0;
            TuiMenu::runMenuLoop(m_documents, m_activeDocIdx, sel, m_topMenus, m_subMenus,
                [this](int m, int it) { executeMenuAction(m, it); });
            return;
        }
        if (ch == KEY_F(12) || ch == 17) { promptQuit(); return; }

        if (ch == 27) {
            nodelay(stdscr, TRUE);
            int nextCh = getch();
            nodelay(stdscr, FALSE);
            if (nextCh == 's' || nextCh == 'S') {
                TuiActions::handleSpellCheck(doc());
                return;
            }
            if (nextCh != ERR) {
                ungetch(nextCh);
            }
        }

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
        if (ch == 26 || ch == 5) {
            TuiActions::handleUndo(doc());
            return;
        }

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

        if (ch == KEY_BTAB) {
            TuiActions::handleIndent(doc(), false);
            return;
        }

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
            int pageJump = std::max(1, LINES - 4);
            doc().cursor.line = std::max(0, doc().cursor.line - pageJump);
            doc().cursor.col = std::min(doc().cursor.col, static_cast<int>(doc().lines[doc().cursor.line].length()));
        } else if (ch == KEY_NPAGE) {
            int pageJump = std::max(1, LINES - 4);
            doc().cursor.line = std::min(static_cast<int>(doc().lines.size()) - 1, doc().cursor.line + pageJump);
            doc().cursor.col = std::min(doc().cursor.col, static_cast<int>(doc().lines[doc().cursor.line].length()));
        } else if (ch == KEY_HOME) {
            doc().cursor.col = 0;
        } else if (ch == KEY_END) {
            doc().cursor.col = static_cast<int>(doc().lines[doc().cursor.line].length());
        } else if (ch == KEY_DC) {
            if (doc().hasSelection) {
                TuiActions::handleDeleteSelection(doc());
                return;
            }
            doc().pushUndo();
            if (doc().cursor.col < static_cast<int>(doc().lines[doc().cursor.line].length())) {
                doc().lines[doc().cursor.line].erase(doc().cursor.col, 1);
                doc().modified = true;
            } else if (doc().cursor.line + 1 < static_cast<int>(doc().lines.size())) {
                doc().lines[doc().cursor.line] += doc().lines[doc().cursor.line + 1];
                doc().lines.erase(doc().lines.begin() + doc().cursor.line + 1);
                doc().modified = true;
            }
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (doc().hasSelection) {
                TuiActions::handleDeleteSelection(doc());
                return;
            }
            if (doc().mode == TuiMode::Code && TuiActions::handleSmartBackspace(doc())) {
                return;
            }

            if (doc().mode == TuiMode::RichText) {
                std::string &cur = doc().lines[doc().cursor.line];
                if (cur.rfind("• ", 0) == 0 && doc().cursor.col <= static_cast<int>(std::string("• ").length())) {
                    doc().pushUndo();
                    cur.erase(0, std::string("• ").length());
                    doc().cursor.col = 0;
                    doc().modified = true;
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
                    if (allDigits && doc().cursor.col <= static_cast<int>(dotPos + 2)) {
                        doc().pushUndo();
                        cur.erase(0, dotPos + 2);
                        doc().cursor.col = 0;
                        doc().modified = true;
                        return;
                    }
                }
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
            if (doc().hasSelection) {
                TuiActions::handleDeleteSelection(doc());
            }
            if (doc().mode == TuiMode::Code) {
                TuiActions::handleSmartReturn(doc());
            } else {
                doc().pushUndo();
                std::string cur = doc().lines[doc().cursor.line];
                std::string rem = cur.substr(doc().cursor.col);
                std::string prefix = cur.substr(0, doc().cursor.col);

                if (prefix == "• ") {
                    doc().lines[doc().cursor.line] = "";
                    doc().cursor.col = 0;
                    doc().modified = true;
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
                            doc().lines[doc().cursor.line] = "";
                            doc().cursor.col = 0;
                            doc().modified = true;
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

                doc().lines[doc().cursor.line] = prefix;
                doc().lines.insert(doc().lines.begin() + doc().cursor.line + 1, nextLinePrefix + rem);
                doc().cursor.line++;
                doc().cursor.col = static_cast<int>(nextLinePrefix.length());
                doc().modified = true;
            }
        } else if (ch >= 32 && ch <= 126) {
            if (doc().hasSelection) {
                TuiActions::handleDeleteSelection(doc());
            } else if (doc().undoStack.empty() || ch == ' ' || doc().cursor.col == 0) {
                doc().pushUndo();
            }
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
