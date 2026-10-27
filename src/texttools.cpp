#include "texttools.h"

#include <QTextEdit>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <algorithm>

namespace TextTools {

// Helper: Safely resolve the affected block range without altering cursor state
static void getBlockRange(QTextEdit *editor, int &startBlockNum, int &endBlockNum) {
    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        int startPos = qMin(cursor.selectionStart(), cursor.selectionEnd());
        int endPos = qMax(cursor.selectionStart(), cursor.selectionEnd());

        QTextCursor startCursor(editor->document());
        startCursor.setPosition(startPos);
        startBlockNum = startCursor.blockNumber();

        QTextCursor endCursor(editor->document());
        endCursor.setPosition(endPos);
        // Exclude the next block if the selection ended right at its start boundary
        if (endCursor.atBlockStart() && endPos > startPos) {
            endBlockNum = endCursor.blockNumber() - 1;
        } else {
            endBlockNum = endCursor.blockNumber();
        }
    } else {
        startBlockNum = 0;
        endBlockNum = editor->document()->blockCount() - 1;
    }
}

struct FragmentData {
    QString text;
    QTextCharFormat format;
};

struct LineBlockData {
    QTextBlockFormat blockFormat;
    QList<FragmentData> fragments;
    QString plainText;
};

// -------------------------------------------------------------------------
// Reorder operations: Sort, Reverse
// Swaps contents into existing blocks without creating/deleting block separators
// -------------------------------------------------------------------------
static void reorderBlocks(QTextEdit *editor, std::function<void(QList<LineBlockData>&)> reorderFn) {
    if (!editor) return;

    int startBlockNum, endBlockNum;
    getBlockRange(editor, startBlockNum, endBlockNum);

    if (startBlockNum >= endBlockNum) return;

    // 1. Snapshot original lines and their exact rich-text fragments
    QList<LineBlockData> lines;
    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = editor->document()->findBlockByNumber(i);
        LineBlockData ld;
        ld.plainText = blk.text();
        ld.blockFormat = blk.blockFormat();

        for (QTextBlock::iterator it = blk.begin(); !it.atEnd(); ++it) {
            QTextFragment frag = it.fragment();
            if (frag.isValid()) {
                ld.fragments.append({frag.text(), frag.charFormat()});
            }
        }
        lines.append(ld);
    }

    // 2. Perform the reorder logic
    reorderFn(lines);

    // 3. Apply changes inside a single undo block without touching block separators
    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();

    for (int i = 0; i < lines.size(); ++i) {
        QTextBlock targetBlk = editor->document()->findBlockByNumber(startBlockNum + i);
        const LineBlockData &src = lines.at(i);

        // Clear line content without removing the block separator
        QTextCursor blkCursor(targetBlk);
        blkCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        blkCursor.removeSelectedText();

        // Restore paragraph styling (e.g. alignment)
        blkCursor.setBlockFormat(src.blockFormat);

        // Re-insert exact styled fragments
        for (const auto &frag : src.fragments) {
            blkCursor.insertText(frag.text, frag.format);
        }
    }

    cursor.endEditBlock();
}

// -------------------------------------------------------------------------
// In-place text transforms: UPPERCASE, lowercase, Title Case, Tabs/Spaces
// Modifies individual fragments from right-to-left to keep offsets stable
// -------------------------------------------------------------------------
static void transformFragments(QTextEdit *editor, std::function<QString(const QString&)> transformFn) {
    if (!editor) return;

    int startBlockNum, endBlockNum;
    getBlockRange(editor, startBlockNum, endBlockNum);

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();

    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = editor->document()->findBlockByNumber(i);
        if (!blk.isValid()) continue;

        struct Snapshot {
            int pos;
            int len;
            QString text;
            QTextCharFormat format;
        };
        QList<Snapshot> snp;

        for (QTextBlock::iterator it = blk.begin(); !it.atEnd(); ++it) {
            QTextFragment frag = it.fragment();
            if (frag.isValid()) {
                snp.append({frag.position(), frag.length(), frag.text(), frag.charFormat()});
            }
        }

        // Right to left prevents index shifting
        for (int s = snp.size() - 1; s >= 0; --s) {
            const Snapshot &item = snp.at(s);
            QString modified = transformFn(item.text);
            if (modified != item.text) {
                QTextCursor fc(editor->document());
                fc.setPosition(item.pos);
                fc.setPosition(item.pos + item.len, QTextCursor::KeepAnchor);
                fc.insertText(modified, item.format);
            }
        }
    }

    cursor.endEditBlock();
}

// -------------------------------------------------------------------------
// Line Edge Operations: Indent, Unindent, Prefix, Suffix
// -------------------------------------------------------------------------
void indent(QTextEdit *editor) {
    if (!editor) return;
    int startBlockNum, endBlockNum;
    getBlockRange(editor, startBlockNum, endBlockNum);

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();

    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = editor->document()->findBlockByNumber(i);
        if (!blk.isValid()) continue;

        QTextCursor c(blk);
        QTextCharFormat fmt = (blk.begin() != blk.end()) ? blk.begin().fragment().charFormat() : QTextCharFormat();
        c.insertText("    ", fmt);
    }

    cursor.endEditBlock();
}

void unindent(QTextEdit *editor) {
    if (!editor) return;
    int startBlockNum, endBlockNum;
    getBlockRange(editor, startBlockNum, endBlockNum);

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();

    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = editor->document()->findBlockByNumber(i);
        if (!blk.isValid()) continue;

        QString t = blk.text();
        int cut = 0;
        if (t.startsWith("    ")) cut = 4;
        else if (t.startsWith('\t')) cut = 1;

        if (cut > 0) {
            QTextCursor c(blk);
            c.setPosition(blk.position() + cut, QTextCursor::KeepAnchor);
            c.removeSelectedText();
        }
    }

    cursor.endEditBlock();
}

void stripBlankLines(QTextEdit *editor) {
    if (!editor) return;
    int startBlockNum, endBlockNum;
    getBlockRange(editor, startBlockNum, endBlockNum);

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();

    // Bottom-to-top traversal so removing lines does not shift block indices
    for (int i = endBlockNum; i >= startBlockNum; --i) {
        QTextBlock blk = editor->document()->findBlockByNumber(i);
        if (blk.isValid() && blk.text().trimmed().isEmpty() && editor->document()->blockCount() > 1) {
            QTextCursor c(blk);
            c.select(QTextCursor::BlockUnderCursor);
            c.removeSelectedText();
        }
    }

    cursor.endEditBlock();
}

void sortLines(QTextEdit *editor) {
    reorderBlocks(editor, [](QList<LineBlockData> &lines) {
        std::sort(lines.begin(), lines.end(), [](const LineBlockData &a, const LineBlockData &b) {
            return QString::compare(a.plainText, b.plainText, Qt::CaseInsensitive) < 0;
        });
    });
}

void reverseLines(QTextEdit *editor) {
    reorderBlocks(editor, [](QList<LineBlockData> &lines) {
        std::reverse(lines.begin(), lines.end());
    });
}

void toUppercase(QTextEdit *editor) {
    transformFragments(editor, [](const QString &t) { return t.toUpper(); });
}

void toLowercase(QTextEdit *editor) {
    transformFragments(editor, [](const QString &t) { return t.toLower(); });
}

void toTitleCase(QTextEdit *editor) {
    transformFragments(editor, [](const QString &t) {
        QString res;
        bool cap = true;
        for (int i = 0; i < t.length(); ++i) {
            QChar ch = t[i];
            if (ch.isSpace()) {
                cap = true;
                res.append(ch);
            } else if (cap) {
                res.append(ch.toTitleCase());
                cap = false;
            } else {
                res.append(ch.toLower());
            }
        }
        return res;
    });
}

void addPrefix(QTextEdit *editor, const QString &prefix) {
    if (!editor || prefix.isEmpty()) return;
    int startBlockNum, endBlockNum;
    getBlockRange(editor, startBlockNum, endBlockNum);

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();

    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = editor->document()->findBlockByNumber(i);
        if (!blk.isValid()) continue;

        QTextCursor c(blk);
        QTextCharFormat fmt = (blk.begin() != blk.end()) ? blk.begin().fragment().charFormat() : QTextCharFormat();
        c.insertText(prefix, fmt);
    }

    cursor.endEditBlock();
}

void addSuffix(QTextEdit *editor, const QString &suffix) {
    if (!editor || suffix.isEmpty()) return;
    int startBlockNum, endBlockNum;
    getBlockRange(editor, startBlockNum, endBlockNum);

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();

    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = editor->document()->findBlockByNumber(i);
        if (!blk.isValid()) continue;

        QTextCursor c(blk);
        c.movePosition(QTextCursor::EndOfBlock);
        QTextCharFormat fmt = (blk.begin() != blk.end()) ? blk.charFormat() : QTextCharFormat();
        c.insertText(suffix, fmt);
    }

    cursor.endEditBlock();
}

void tabsToSpaces(QTextEdit *editor) {
    transformFragments(editor, [](const QString &t) {
        QString s = t;
        return s.replace('\t', "    ");
    });
}

void spacesToTabs(QTextEdit *editor) {
    transformFragments(editor, [](const QString &t) {
        QString s = t;
        while (s.contains("    ")) {
            s.replace("    ", "\t");
        }
        return s;
    });
}

} // namespace TextTools
