#ifndef MICROWRITER_TUIACTIONS_H
#define MICROWRITER_TUIACTIONS_H

#include "tuitypes.h"
#include <string>

namespace TuiActions {
bool isMarkdownDoc(const EditorDocument &doc);

void injectTagAroundSelection(EditorDocument &doc, const std::string &openTag, const std::string &closeTag);
void applyBold(EditorDocument &doc);
void applyItalic(EditorDocument &doc);
void applyUnderline(EditorDocument &doc);

void handleFontSelection(EditorDocument &doc); 
void handleFontSizeSelection(EditorDocument &doc); 
void handleColorSelection(EditorDocument &doc); 
void handleInsertTable(EditorDocument &doc); 
void handleInsertBulletList(EditorDocument &doc); 
void handleInsertNumberedList(EditorDocument &doc); 
void handleInsertHorizontalRule(EditorDocument &doc);
void handleInsertBlockquote(EditorDocument &doc);

void handleDeleteSelection(EditorDocument &doc);
void handleCutCopy(EditorDocument &doc, bool isCut);
void handlePaste(EditorDocument &doc);
void handleUndo(EditorDocument &doc);
void handleRedo(EditorDocument &doc);
void handleFindReplace(EditorDocument &doc, const std::string &initFind = "", const std::string &initReplace = "");
void handleInsertImage(EditorDocument &doc);

void handleIndent(EditorDocument &doc, bool indent);
void handleStripBlankLines(EditorDocument &doc);
void handleSortLines(EditorDocument &doc);
void handleReverseLines(EditorDocument &doc);
void handleChangeCase(EditorDocument &doc, bool upper);
void handleToggleComment(EditorDocument &doc);
void handleDuplicate(EditorDocument &doc);
void handleMoveLine(EditorDocument &doc, bool moveUp);
bool handleSmartBackspace(EditorDocument &doc);
void handleSmartReturn(EditorDocument &doc);
void handleSpellCheck(EditorDocument &doc);
void handleStripTrailingSpaces(EditorDocument &doc);
} 

#endif // MICROWRITER_TUIACTIONS_H