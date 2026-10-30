#include "tuiconverter.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace TuiConverter {

std::string stripHtmlHeadersAndStyle(std::string text) {
    text = std::regex_replace(text, std::regex(R"(<style[^>]*>[\s\S]*?</style>)", std::regex::icase), "");
    text = std::regex_replace(text, std::regex(R"(<head[^>]*>[\s\S]*?</head>)", std::regex::icase), "");
    return text;
}

std::string formatInlineMarkdownToHtml(std::string text) {
    text = std::regex_replace(text, std::regex(R"(`([^`]+)`)"), "<code>$1</code>");
    text = std::regex_replace(text, std::regex(R"(\*\*([^*]+)\*\*)"), "<strong>$1</strong>");
    text = std::regex_replace(text, std::regex(R"(__([^_]+)__)"), "<strong>$1</strong>");
    text = std::regex_replace(text, std::regex(R"(\*([^*]+)\*)"), "<em>$1</em>");
    text = std::regex_replace(text, std::regex(R"(_([^_]+)_)"), "<em>$1</em>");
    return text;
}

std::string formatInlineHtmlToMarkdown(std::string text) {
    text = std::regex_replace(text, std::regex(R"(<code[^>]*>([\s\S]*?)</code>)"), "`$1`");
    text = std::regex_replace(text, std::regex(R"(<(strong|b)[^>]*>([\s\S]*?)</\1>)"), "**$2**");
    text = std::regex_replace(text, std::regex(R"(<(em|i)[^>]*>([\s\S]*?)</\1>)"), "*$2*");
    text = std::regex_replace(text, std::regex(R"(<span[^>]*font-weight:\s*(700|bold)[^>]*>([\s\S]*?)</span>)"), "**$2**");
    text = std::regex_replace(text, std::regex(R"(<span[^>]*>([\s\S]*?)</span>)"), "$1");
    return text;
}

std::vector<std::vector<std::string>> parseHtmlTableRows(const std::string &tableHtml) {
    std::string normalized = tableHtml;

    normalized = std::regex_replace(normalized, std::regex(R"(^[\s\S]*?(?=<tr\b))", std::regex::icase), "");
    normalized = std::regex_replace(normalized, std::regex(R"(<table\b[^>]*>)", std::regex::icase), "");
    normalized = std::regex_replace(normalized, std::regex(R"(</table>\s*)", std::regex::icase), "");
    normalized = std::regex_replace(normalized, std::regex(R"(<tbody\b[^>]*>)", std::regex::icase), "");
    normalized = std::regex_replace(normalized, std::regex(R"(</tbody>\s*)", std::regex::icase), "");
    normalized = std::regex_replace(normalized, std::regex(R"(<thead\b[^>]*>)", std::regex::icase), "");
    normalized = std::regex_replace(normalized, std::regex(R"(</thead>\s*)", std::regex::icase), "");
    normalized = std::regex_replace(normalized, std::regex(R"(<tfoot\b[^>]*>)", std::regex::icase), "");
    normalized = std::regex_replace(normalized, std::regex(R"(</tfoot>\s*)", std::regex::icase), "");

    std::regex trRegex(R"(<tr\b[^>]*>([\s\S]*?)</tr>\s*)", std::regex::icase);
    std::regex cellRegex(R"(<(td|th)\b[^>]*>([\s\S]*?)</\1\s*>)", std::regex::icase);

    std::vector<std::vector<std::string>> rows;
    auto trBegin = std::sregex_iterator(normalized.begin(), normalized.end(), trRegex);
    auto trEnd = std::sregex_iterator();

    for (auto tri = trBegin; tri != trEnd; ++tri) {
        std::string rowHtml = (*tri)[1].str();
        rowHtml = std::regex_replace(rowHtml, std::regex(R"(^\s*<tr\b[^>]*>)", std::regex::icase), "");
        rowHtml = std::regex_replace(rowHtml, std::regex(R"(</tr>\s*$)"), "");

        std::vector<std::string> cells;
        auto cellBegin = std::sregex_iterator(rowHtml.begin(), rowHtml.end(), cellRegex);
        auto cellEnd = std::sregex_iterator();

        for (auto ci = cellBegin; ci != cellEnd; ++ci) {
            std::string cellText = (*ci)[2].str();

            cellText = std::regex_replace(cellText, std::regex(R"(^\s*<tr\b[^>]*>)", std::regex::icase), "");
            cellText = std::regex_replace(cellText, std::regex(R"(</tr>\s*$)"), "");
            cellText = std::regex_replace(cellText, std::regex(R"(^\s*<(?:td|th)\b[^>]*>)", std::regex::icase), "");
            cellText = std::regex_replace(cellText, std::regex(R"(</(?:td|th)\s*>\s*$)"), "");

            cellText = std::regex_replace(cellText, std::regex(R"(</?p[^>]*>)", std::regex::icase), "");
            cellText = std::regex_replace(cellText, std::regex(R"(<br\s*/?>)", std::regex::icase), "\n");
            cellText = std::regex_replace(cellText, std::regex(R"(\s*\n\s*)"), " ");
            cellText = std::regex_replace(cellText, std::regex(R"(^\s+|\s+$)"), "");
            cells.push_back(formatInlineHtmlToMarkdown(cellText));
        }

        if (!cells.empty()) {
            rows.push_back(cells);
        }
    }

    return rows;
}

static size_t findMatchingTableEnd(const std::string &html, size_t startPos) {
    size_t depth = 0;
    size_t pos = startPos;
    while (pos < html.length()) {
        size_t nextOpen = html.find("<table", pos);
        size_t nextClose = html.find("</table>", pos);

        if (nextClose == std::string::npos) return std::string::npos;

        if (nextOpen != std::string::npos && nextOpen < nextClose) {
            depth++;
            pos = nextOpen + 6;
        } else {
            depth--;
            if (depth == 0) {
                return nextClose;
            }
            pos = nextClose + 8;
        }
    }
    return std::string::npos;
}

std::string serializeLinesToMarkdown(const std::vector<std::string> &lines) {
    std::ostringstream out;
    size_t i = 0;
    bool inStyle = false;

    while (i < lines.size()) {
        std::string l = lines[i];

        if (l.find("<style") != std::string::npos) {
            inStyle = true;
            if (l.find("</style>") != std::string::npos) inStyle = false;
            ++i;
            continue;
        }
        if (inStyle) {
            if (l.find("</style>") != std::string::npos) inStyle = false;
            ++i;
            continue;
        }

        if (l.find("<table") != std::string::npos) {
            if (l.find("-qt-table-type: root") != std::string::npos || l.find("-qt-table-type:root") != std::string::npos) {
                ++i;
                continue;
            }

            std::string tableContent;
            size_t depth = 0;
            while (i < lines.size()) {
                tableContent += lines[i] + "\n";
                if (lines[i].find("<table") != std::string::npos) {
                    depth++;
                }
                if (lines[i].find("</table>") != std::string::npos) {
                    if (depth > 0) depth--;
                    if (depth == 0) break;
                }
                ++i;
            }

            auto rows = parseHtmlTableRows(tableContent);
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

        std::regex hRegex(R"(<h([1-6])\b[^>]*>([\s\S]*?)</h\1\s*>)", std::regex::icase);
        std::smatch hMatch;
        if (std::regex_search(l, hMatch, hRegex)) {
            int level = std::stoi(hMatch[1].str());
            std::string content = hMatch[2].str();
            std::string prefix(level, '#');
            out << prefix << " " << formatInlineHtmlToMarkdown(content) << "\n\n";
            ++i;
            continue;
        }

        std::regex liRegex(R"(<li\b[^>]*>([\s\S]*?)</li>\s*)", std::regex::icase);
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
            std::string q = std::regex_replace(l, std::regex(R"(</?blockquote[^>]*)", std::regex::icase), "");
            size_t first = q.find_first_not_of(" \t");
            q = (first != std::string::npos) ? ("> " + q.substr(first)) : "> ";
            out << formatInlineHtmlToMarkdown(q) << "\n\n";
            ++i;
            continue;
        }

        std::regex pRegex(R"(<p\b[^>]*>([\s\S]*?)</p\s*>)", std::regex::icase);
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
            l.find("</body>") != std::string::npos || l.find("<ul") != std::string::npos ||
            l.find("</ul>") != std::string::npos || l.find("<ol") != std::string::npos ||
            l.find("ol>") != std::string::npos) {
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
            } else if (trimmed.find("<table") != std::string::npos || trimmed.find("<tr") != std::string::npos || trimmed.find("<td") != std::string::npos ||
                       trimmed.find("<h") != std::string::npos || trimmed.find("<hr") != std::string::npos || trimmed.find("<blockquote") != std::string::npos) {
                out << l << "\n";
            } else if (trimmed.rfind("<p", 0) == 0 && trimmed.find("</p>") != std::string::npos) {
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

        // Strip HTML horizontal rules and blockquotes
        if (l.find("<hr") != std::string::npos) {
            l = std::regex_replace(l, std::regex(R"(<hr\s*/?>)", std::regex::icase), "----------------------------------------");
        } else if (l.find("<blockquote") != std::string::npos) {
            l = std::regex_replace(l, std::regex(R"(</?blockquote[^>]*>)", std::regex::icase), "");
            size_t first = l.find_first_not_of(" \t");
            l = (first != std::string::npos) ? ("> " + l.substr(first)) : "> ";
        }

        // Strip HTML tags
        l = std::regex_replace(l, std::regex(R"(<[^>]*>)"), "");

        // Convert bullet point markers to clean dashes
        if (l.rfind("• ", 0) == 0) {
            l = "- " + l.substr(std::string("• ").length());
        }

        // Strip Markdown headers (e.g., "# Title" -> "Title")
        l = std::regex_replace(l, std::regex(R"(^[ \t]*#+[ \t]+)"), "");

        // Strip Markdown images & links: ![alt](url) -> alt, [text](url) -> text
        l = std::regex_replace(l, std::regex(R"(!?\[([^\]]*)\]\([^)]*\))"), "$1");

        // Strip bold & italic formatting: ***text***, **text**, *text*, ___text___, __text__, _text_
        l = std::regex_replace(l, std::regex(R"(\*\*\*([^*]+)\*\*\*)"), "$1");
        l = std::regex_replace(l, std::regex(R"(\*\*([^*]+)\*\*)"), "$1");
        l = std::regex_replace(l, std::regex(R"(\*([^*]+)\*)"), "$1");
        l = std::regex_replace(l, std::regex(R"(_{3}([^_]+)_{3})"), "$1");
        l = std::regex_replace(l, std::regex(R"(_{2}([^_]+)_{2})"), "$1");
        l = std::regex_replace(l, std::regex(R"(_([^_]+)_)"), "$1");

        // Strip inline code and strikethrough: `code` -> code, ~~text~~ -> text
        l = std::regex_replace(l, std::regex(R"(`([^`]+)`)"), "$1");
        l = std::regex_replace(l, std::regex(R"(~~([^~]+)~~)"), "$1");

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

void deserializeRawToRich(EditorDocument &d, std::string rawContent) {
    std::string ext = "";
    size_t dot = d.filePath.find_last_of('.');
    if (dot != std::string::npos) {
        ext = d.filePath.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    d.lines.clear();

    bool isHtml = (ext == "html" || ext == "htm" || d.filePath.empty());
    if (!isHtml && (rawContent.find("<html") != std::string::npos || rawContent.find("<!DOCTYPE") != std::string::npos)) {
        isHtml = true;
    }

    if (isHtml) {
        rawContent = stripHtmlHeadersAndStyle(rawContent);
    }

    if (isHtml && (rawContent.find("<p") != std::string::npos || rawContent.find("<li") != std::string::npos ||
        rawContent.find("<h1") != std::string::npos || rawContent.find("<h2") != std::string::npos ||
        rawContent.find("<h3") != std::string::npos || rawContent.find("<h4") != std::string::npos ||
        rawContent.find("<h5") != std::string::npos || rawContent.find("<h6") != std::string::npos ||
        rawContent.find("<table") != std::string::npos || rawContent.find("<hr") != std::string::npos ||
        rawContent.find("<blockquote") != std::string::npos)) {
        size_t pos = 0;
        int currentOlNum = 1;
        bool insideOl = false;

        while (pos < rawContent.length()) {
            size_t nextTag = rawContent.find('<', pos);
            if (nextTag == std::string::npos) break;

            if (rawContent.compare(nextTag, 6, "<style", 6) == 0) {
                size_t endStyle = rawContent.find("</style>", nextTag);
                if (endStyle != std::string::npos) {
                    pos = endStyle + 8;
                    continue;
                }
            }
            if (rawContent.compare(nextTag, 5, "<head", 5) == 0) {
                size_t endHead = rawContent.find("</head>", nextTag);
                if (endHead != std::string::npos) {
                    pos = endHead + 7;
                    continue;
                }
            }

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

            if (nextTag + 3 < rawContent.length() &&
                (rawContent[nextTag + 1] == 'h' || rawContent[nextTag + 1] == 'H') &&
                (rawContent[nextTag + 2] >= '1' && rawContent[nextTag + 2] <= '6') &&
                (rawContent[nextTag + 3] == '>' || rawContent[nextTag + 3] == ' ')) {
                char hDigit = rawContent[nextTag + 2];
                size_t closeTag = rawContent.find('>', nextTag);
                if (closeTag == std::string::npos) break;
                std::string endTag = std::string("</h") + hDigit + ">";
                size_t endH = rawContent.find(endTag, closeTag);
                if (endH == std::string::npos) {
                    endTag = std::string("</H") + hDigit + ">";
                    endH = rawContent.find(endTag, closeTag);
                }
                if (endH != std::string::npos) {
                    std::string hContent = rawContent.substr(closeTag + 1, endH - closeTag - 1);
                    int level = hDigit - '0';
                    d.lines.push_back(std::string(level, '#') + " " + hContent);
                    pos = endH + endTag.length();
                    continue;
                }
            }

            if (rawContent.compare(nextTag, 3, "<hr", 3) == 0 &&
                (rawContent[nextTag + 3] == '>' || rawContent[nextTag + 3] == ' ' || rawContent[nextTag + 3] == '/')) {
                size_t closeTag = rawContent.find('>', nextTag);
                if (closeTag != std::string::npos) {
                    d.lines.push_back("<hr>");
                    pos = closeTag + 1;
                    continue;
                }
            }

            if (rawContent.compare(nextTag, 11, "<blockquote", 11) == 0 &&
                (rawContent[nextTag + 11] == '>' || rawContent[nextTag + 11] == ' ')) {
                size_t closeTag = rawContent.find('>', nextTag);
                if (closeTag == std::string::npos) break;
                size_t endBq = rawContent.find("</blockquote>", closeTag);
                if (endBq != std::string::npos) {
                    std::string bqContent = rawContent.substr(closeTag + 1, endBq - closeTag - 1);
                    d.lines.push_back("<blockquote>" + bqContent + "</blockquote>");
                    pos = endBq + 13;
                    continue;
                }
            }

            if (rawContent.compare(nextTag, 6, "<table", 6) == 0 &&
                (rawContent[nextTag + 6] == '>' || rawContent[nextTag + 6] == ' ')) {
                size_t tableTagEnd = rawContent.find('>', nextTag);
                if (tableTagEnd != std::string::npos) {
                    std::string tagHeader = rawContent.substr(nextTag, tableTagEnd - nextTag + 1);
                    if (tagHeader.find("-qt-table-type: root") != std::string::npos ||
                        tagHeader.find("-qt-table-type:root") != std::string::npos) {
                        pos = tableTagEnd + 1;
                        continue;
                    }
                }

                size_t endTable = findMatchingTableEnd(rawContent, nextTag);
                if (endTable != std::string::npos) {
                    std::string tableContent = rawContent.substr(nextTag, endTable + 8 - nextTag);
                    auto rows = parseHtmlTableRows(tableContent);

                    if (!rows.empty()) {
                        size_t maxCols = 0;
                        for (const auto &r : rows) maxCols = std::max(maxCols, r.size());

                        std::string headerLine = "|";
                        for (size_t c = 0; c < maxCols; ++c) {
                            std::string val = (c < rows[0].size()) ? rows[0][c] : " ";
                            headerLine += " " + val + " |";
                        }
                        d.lines.push_back(headerLine);

                        std::string sepLine = "|";
                        for (size_t c = 0; c < maxCols; ++c) {
                            sepLine += " --- |";
                        }
                        d.lines.push_back(sepLine);

                        for (size_t r = 1; r < rows.size(); ++r) {
                            std::string rowLine = "|";
                            for (size_t c = 0; c < maxCols; ++c) {
                                std::string val = (c < rows[r].size()) ? rows[r][c] : " ";
                                rowLine += " " + val + " |";
                            }
                            d.lines.push_back(rowLine);
                        }
                    }

                    pos = endTable + 8;
                    continue;
                }
            }

            if (rawContent.compare(nextTag, 8, "</table>", 8) == 0) {
                pos = nextTag + 8;
                continue;
            }
            if (rawContent.compare(nextTag, 4, "<tr>", 4) == 0 || rawContent.compare(nextTag, 4, "<tr ", 4) == 0) {
                pos = rawContent.find('>', nextTag) + 1;
                continue;
            }
            if (rawContent.compare(nextTag, 5, "</tr>", 5) == 0) {
                pos = nextTag + 5;
                continue;
            }
            if (rawContent.compare(nextTag, 4, "<td>", 4) == 0 || rawContent.compare(nextTag, 4, "<td ", 4) == 0) {
                pos = rawContent.find('>', nextTag) + 1;
                continue;
            }
            if (rawContent.compare(nextTag, 5, "</td>", 5) == 0) {
                pos = nextTag + 5;
                continue;
            }

            if (rawContent.compare(nextTag, 2, "<p", 2) == 0 &&
                (rawContent[nextTag + 2] == '>' || rawContent[nextTag + 2] == ' ')) {
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

            if (rawContent.compare(nextTag, 3, "<li", 3) == 0 &&
                (rawContent[nextTag + 3] == '>' || rawContent[nextTag + 3] == ' ')) {
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
        bool inStyleTag = false;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (line.find("<style") != std::string::npos) {
                inStyleTag = true;
                if (line.find("</style>") != std::string::npos) inStyleTag = false;
                continue;
            }
            if (inStyleTag) {
                if (line.find("</style>") != std::string::npos) inStyleTag = false;
                continue;
            }
            if (line.find("<head") != std::string::npos || line.find("</head>") != std::string::npos ||
                line.find("<!DOCTYPE") != std::string::npos || line.find("<html") != std::string::npos ||
                line.find("</html>") != std::string::npos || line.find("<body") != std::string::npos ||
                line.find("</body>") != std::string::npos) {
                continue;
            }

            if (ext == "md" || ext == "txt" || rawContent.find("<html") == std::string::npos) {
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

} // namespace TuiConverter
