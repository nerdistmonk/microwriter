#include "texttools.h"

#include <QTextEdit>
#include <QTextCursor>
#include <algorithm>

namespace TextTools {

void modifySelectedLines(QTextEdit *editor, std::function<QStringList(QStringList)> transformer) {
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();

    QString text;
    bool hasSelection = cursor.hasSelection();
    if (hasSelection) {
        text = cursor.selectedText();
    } else {
        cursor.select(QTextCursor::Document);
        text = cursor.selectedText();
    }

    text.replace(QChar(0x2029), '\n');
    QStringList lines = text.split('\n');
    QStringList transformed = transformer(lines);
    QString newText = transformed.join('\n');

    cursor.beginEditBlock();
    if (hasSelection) {
        cursor.insertText(newText);
    } else {
        cursor.select(QTextCursor::Document);
        cursor.insertText(newText);
    }
    cursor.endEditBlock();
}

void indent(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) out.append("    " + l);
        return out;
    });
}

void unindent(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        QStringList out;
        for (QString l : lines) {
            if (l.startsWith("    ")) {
                l.remove(0, 4);
            } else if (l.startsWith('\t')) {
                l.remove(0, 1);
            }
            out.append(l);
        }
        return out;
    });
}

void stripBlankLines(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) {
            if (!l.trimmed().isEmpty()) {
                out.append(l);
            }
        }
        return out;
    });
}

void reverseLines(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        std::reverse(lines.begin(), lines.end());
        return lines;
    });
}

void sortLines(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        std::sort(lines.begin(), lines.end(), [](const QString &a, const QString &b) {
            return QString::compare(a, b, Qt::CaseInsensitive) < 0;
        });
        return lines;
    });
}

void toUppercase(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) out.append(l.toUpper());
        return out;
    });
}

void toLowercase(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) out.append(l.toLower());
        return out;
    });
}

void toTitleCase(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) {
            QString title;
            bool capitalizeNext = true;
            for (int i = 0; i < l.length(); ++i) {
                QChar c = l[i];
                if (c.isSpace()) {
                    capitalizeNext = true;
                    title.append(c);
                } else {
                    if (capitalizeNext) {
                        title.append(c.toTitleCase());
                        capitalizeNext = false;
                    } else {
                        title.append(c.toLower());
                    }
                }
            }
            out.append(title);
        }
        return out;
    });
}

void addPrefix(QTextEdit *editor, const QString &prefix) {
    modifySelectedLines(editor, [prefix](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) out.append(prefix + l);
        return out;
    });
}

void addSuffix(QTextEdit *editor, const QString &suffix) {
    modifySelectedLines(editor, [suffix](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) out.append(l + suffix);
        return out;
    });
}

void tabsToSpaces(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        QStringList out;
        for (QString l : lines) {
            out.append(l.replace('\t', "    "));
        }
        return out;
    });
}

void spacesToTabs(QTextEdit *editor) {
    modifySelectedLines(editor, [](QStringList lines) {
        QStringList out;
        for (QString l : lines) {
            while (l.startsWith("    ")) {
                l.replace(0, 4, "\t");
            }
            out.append(l);
        }
        return out;
    });
}

} // namespace TextTools
