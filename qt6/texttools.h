#ifndef TEXTTOOLS_H
#define TEXTTOOLS_H

#include <QString>
#include <QStringList>

class QTextEdit;
class QPlainTextEdit;
class QTextDocument;
class QTextCursor;
class BaseEditor;

namespace TextTools {

void indent(BaseEditor *editor);
void unindent(BaseEditor *editor);
void stripBlankLines(BaseEditor *editor);

void reverseLines(BaseEditor *editor);
void sortLines(BaseEditor *editor);

void toUppercase(BaseEditor *editor);
void toLowercase(BaseEditor *editor);
void toTitleCase(BaseEditor *editor);

void addPrefix(BaseEditor *editor, const QString &prefix);
void addSuffix(BaseEditor *editor, const QString &suffix);

void tabsToSpaces(BaseEditor *editor);
void spacesToTabs(BaseEditor *editor);
void removeTrailingSpaces(BaseEditor *editor);

} // namespace TextTools

#endif // TEXTTOOLS_H