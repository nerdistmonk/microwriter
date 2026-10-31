#ifndef TUICONVERTER_H
#define TUICONVERTER_H

#include "tuitypes.h"
#include <string>
#include <vector>

namespace TuiConverter {

// HTML/Markdown inline and block parsing helpers
std::string stripHtmlHeadersAndStyle(std::string text);
std::string formatInlineMarkdownToHtml(std::string text);
std::vector<std::vector<std::string>> parseHtmlTableRows(const std::string &tableHtml);

// Line collection serializations
std::string serializeLinesToHtml(const std::vector<std::string> &lines);
std::string serializeLinesToPlainText(const std::vector<std::string> &lines);

// Full document serialization & deserialization
std::string serializeDocToRaw(const EditorDocument &d);
void deserializeRawToRich(EditorDocument &d, std::string rawContent);

} // namespace TuiConverter

#endif // TUICONVERTER_H
