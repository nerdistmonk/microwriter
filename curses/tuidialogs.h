#ifndef MICROWRITER_TUIDIALOGS_H
#define MICROWRITER_TUIDIALOGS_H

#include <string>
#include <vector>

namespace TuiDialogs {
    std::string showPromptModal(const std::string &prompt);
    int showListSelectionModal(const std::string &title, const std::vector<std::string> &items);
    void showAboutModal();
    bool showFindReplaceModal(std::string &findStr, std::string &replaceStr, bool &doReplace);
    int showRegexCheatSheetModal(std::string &outFind, std::string &outReplace);
    int showHtmlCheatSheetModal(std::string &outTag);
    int showSedUtilsCheatSheetModal(std::string &outSnippet);
    int showBashScriptCheatSheetModal(std::string &outScript);
    int showTabContextMenuModal(const std::string &tabTitle); // returns 0: cancel, 1: save, 2: close, 3: save & close
}

#endif // MICROWRITER_TUIDIALOGS_H
