#ifndef MICROWRITER_TUIDIALOGS_H
#define MICROWRITER_TUIDIALOGS_H

#include <string>
#include <vector>

namespace TuiDialogs {
std::string showPromptModal(const std::string &prompt);
int showListSelectionModal(const std::string &title, const std::vector<std::string> &items);
void showAboutModal();
bool showFindReplaceModal(std::string &findStr, std::string &replaceStr, bool &doReplace);
int showConfirmModal(const std::string &title, const std::string &message); // 1 = Save/Yes, 2 = Discard/No, 0 = Cancel
bool showSaveAsModal(std::string &filePath, int &formatType, bool &stripFormatting); // formatType: 0 = HTML (.html), 1 = Plain Text (.txt)
bool showOpenFileModal(std::string &filePath, int &filterType); // 0 = All Supported, 1 = Source Code, 2 = Documents, 3 = All Files (*.*)
int showSpellSuggestionsModal(const std::string &word, const std::vector<std::string> &suggestions);
int showRegexCheatSheetModal(std::string &outFind, std::string &outReplace);
int showHtmlCheatSheetModal(std::string &outTag);
int showSedUtilsCheatSheetModal(std::string &outSnippet);
int showBashScriptCheatSheetModal(std::string &outScript);
int showTabContextMenuModal(const std::string &tabTitle); // returns 0: cancel, 1: save, 2: close, 3: save & close
}

#endif // MICROWRITER_TUIDIALOGS_H
