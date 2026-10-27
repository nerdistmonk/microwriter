#ifndef TEXTTOOLS_H
#define TEXTTOOLS_H

#include <QString>
#include <QStringList>
#include <functional>

class QTextEdit;

namespace TextTools {

void modifySelectedLines(QTextEdit *editor, std::function<QStringList(QStringList)> transformer);

void indent(QTextEdit *editor);
void unindent(QTextEdit *editor);
void stripBlankLines(QTextEdit *editor);

void reverseLines(QTextEdit *editor);
void sortLines(QTextEdit *editor);

void toUppercase(QTextEdit *editor);
void toLowercase(QTextEdit *editor);
void toTitleCase(QTextEdit *editor);

void addPrefix(QTextEdit *editor, const QString &prefix);
void addSuffix(QTextEdit *editor, const QString &suffix);

void tabsToSpaces(QTextEdit *editor);
void spacesToTabs(QTextEdit *editor);

} // namespace TextTools

#endif // TEXTTOOLS_H
