#include "texttools.h"
#include "baseeditor.h"
#include "editorview.h"
#include "codeeditorview.h"

#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <algorithm>

namespace TextTools {

// Helper: Extract document and text cursor from any BaseEditor
static bool getEditorState(BaseEditor *base, QTextDocument *&doc, QTextCursor &cursor) {
    if (!base) return false;
    if (EditorView *ed = dynamic_cast<EditorView*>(base)) {
        doc = ed->document();
        cursor = ed->textCursor();
        return doc != nullptr;
    } else if (CodeEditorView *ce = dynamic_cast<CodeEditorView*>(base)) {
        doc = ce->document();
        cursor = ce->textCursor();
        return doc != nullptr;
    }
    return false;
}

static void applyCursorBack(BaseEditor *base, const QTextCursor &cursor) {
    if (!base) return;
    if (EditorView *ed = dynamic_cast<EditorView*>(base)) {
        ed->setTextCursor(cursor);
    } else if (CodeEditorView *ce = dynamic_cast<CodeEditorView*>(base)) {
        ce->setTextCursor(cursor);
    }
}

// Helper: Safely resolve the affected block range without altering cursor state
static void getBlockRange(QTextDocument *doc, const QTextCursor &cursor, int &startBlockNum, int &endBlockNum) {
    if (cursor.hasSelection()) {
        int startPos = qMin(cursor.selectionStart(), cursor.selectionEnd());
        int endPos = qMax(cursor.selectionStart(), cursor.selectionEnd());

        QTextCursor startCursor(doc);
        startCursor.setPosition(startPos);
        startBlockNum = startCursor.blockNumber();

        QTextCursor endCursor(doc);
        endCursor.setPosition(endPos);
        if (endCursor.atBlockStart() && endPos > startPos) {
            endBlockNum = endCursor.blockNumber() - 1;
        } else {
            endBlockNum = endCursor.blockNumber();
        }
    } else {
        startBlockNum = 0;
        endBlockNum = doc->blockCount() - 1;
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

static void reorderBlocks(BaseEditor *editor, std::function<void(QList<LineBlockData>&)> reorderFn) {
    QTextDocument *doc = nullptr;
    QTextCursor cursor;
    if (!getEditorState(editor, doc, cursor)) return;

    int startBlockNum, endBlockNum;
    getBlockRange(doc, cursor, startBlockNum, endBlockNum);

    if (startBlockNum >= endBlockNum) return;

    QList<LineBlockData> lines;
    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = doc->findBlockByNumber(i);
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

    reorderFn(lines);

    cursor.beginEditBlock();
    for (int i = 0; i < lines.size(); ++i) {
        QTextBlock targetBlk = doc->findBlockByNumber(startBlockNum + i);
        const LineBlockData &src = lines.at(i);

        QTextCursor blkCursor(targetBlk);
        blkCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        blkCursor.removeSelectedText();

        blkCursor.setBlockFormat(src.blockFormat);
        for (const auto &frag : src.fragments) {
            blkCursor.insertText(frag.text, frag.format);
        }
    }
    cursor.endEditBlock();
    applyCursorBack(editor, cursor);
}

static void transformFragments(BaseEditor *editor, std::function<QString(const QString&)> transformFn) {
    QTextDocument *doc = nullptr;
    QTextCursor cursor;
    if (!getEditorState(editor, doc, cursor)) return;

    int startBlockNum, endBlockNum;
    getBlockRange(doc, cursor, startBlockNum, endBlockNum);

    cursor.beginEditBlock();
    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = doc->findBlockByNumber(i);
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

        for (int s = snp.size() - 1; s >= 0; --s) {
            const Snapshot &item = snp.at(s);
            QString modified = transformFn(item.text);
            if (modified != item.text) {
                QTextCursor fc(doc);
                fc.setPosition(item.pos);
                fc.setPosition(item.pos + item.len, QTextCursor::KeepAnchor);
                fc.insertText(modified, item.format);
            }
        }
    }
    cursor.endEditBlock();
    applyCursorBack(editor, cursor);
}

void indent(BaseEditor *editor) {
    QTextDocument *doc = nullptr;
    QTextCursor cursor;
    if (!getEditorState(editor, doc, cursor)) return;

    int startBlockNum, endBlockNum;
    getBlockRange(doc, cursor, startBlockNum, endBlockNum);

    cursor.beginEditBlock();
    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = doc->findBlockByNumber(i);
        if (!blk.isValid()) continue;

        QTextCursor c(blk);
        QTextCharFormat fmt = (blk.begin() != blk.end()) ? blk.begin().fragment().charFormat() : QTextCharFormat();
        c.insertText("    ", fmt);
    }
    cursor.endEditBlock();
    applyCursorBack(editor, cursor);
}

void unindent(BaseEditor *editor) {
    QTextDocument *doc = nullptr;
    QTextCursor cursor;
    if (!getEditorState(editor, doc, cursor)) return;

    int startBlockNum, endBlockNum;
    getBlockRange(doc, cursor, startBlockNum, endBlockNum);

    cursor.beginEditBlock();
    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = doc->findBlockByNumber(i);
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
    applyCursorBack(editor, cursor);
}

void stripBlankLines(BaseEditor *editor) {
    QTextDocument *doc = nullptr;
    QTextCursor cursor;
    if (!getEditorState(editor, doc, cursor)) return;

    int startBlockNum, endBlockNum;
    getBlockRange(doc, cursor, startBlockNum, endBlockNum);

    cursor.beginEditBlock();
    for (int i = endBlockNum; i >= startBlockNum; --i) {
        QTextBlock blk = doc->findBlockByNumber(i);
        if (blk.isValid() && blk.text().trimmed().isEmpty() && doc->blockCount() > 1) {
            QTextCursor c(blk);
            c.select(QTextCursor::BlockUnderCursor);
            c.removeSelectedText();
        }
    }
    cursor.endEditBlock();
    applyCursorBack(editor, cursor);
}

void sortLines(BaseEditor *editor) {
    reorderBlocks(editor, [](QList<LineBlockData> &lines) {
        std::sort(lines.begin(), lines.end(), [](const LineBlockData &a, const LineBlockData &b) {
            return QString::compare(a.plainText, b.plainText, Qt::CaseInsensitive) < 0;
        });
    });
}

void reverseLines(BaseEditor *editor) {
    reorderBlocks(editor, [](QList<LineBlockData> &lines) {
        std::reverse(lines.begin(), lines.end());
    });
}

void toUppercase(BaseEditor *editor) {
    transformFragments(editor, [](const QString &t) { return t.toUpper(); });
}

void toLowercase(BaseEditor *editor) {
    transformFragments(editor, [](const QString &t) { return t.toLower(); });
}

void toTitleCase(BaseEditor *editor) {
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

void addPrefix(BaseEditor *editor, const QString &prefix) {
    if (!editor || prefix.isEmpty()) return;
    QTextDocument *doc = nullptr;
    QTextCursor cursor;
    if (!getEditorState(editor, doc, cursor)) return;

    int startBlockNum, endBlockNum;
    getBlockRange(doc, cursor, startBlockNum, endBlockNum);

    cursor.beginEditBlock();
    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = doc->findBlockByNumber(i);
        if (!blk.isValid()) continue;

        QTextCursor c(blk);
        QTextCharFormat fmt = (blk.begin() != blk.end()) ? blk.begin().fragment().charFormat() : QTextCharFormat();
        c.insertText(prefix, fmt);
    }
    cursor.endEditBlock();
    applyCursorBack(editor, cursor);
}

void addSuffix(BaseEditor *editor, const QString &suffix) {
    if (!editor || suffix.isEmpty()) return;
    QTextDocument *doc = nullptr;
    QTextCursor cursor;
    if (!getEditorState(editor, doc, cursor)) return;

    int startBlockNum, endBlockNum;
    getBlockRange(doc, cursor, startBlockNum, endBlockNum);

    cursor.beginEditBlock();
    for (int i = startBlockNum; i <= endBlockNum; ++i) {
        QTextBlock blk = doc->findBlockByNumber(i);
        if (!blk.isValid()) continue;

        QTextCursor c(blk);
        c.movePosition(QTextCursor::EndOfBlock);
        QTextCharFormat fmt = (blk.begin() != blk.end()) ? blk.charFormat() : QTextCharFormat();
        c.insertText(suffix, fmt);
    }
    cursor.endEditBlock();
    applyCursorBack(editor, cursor);
}

void tabsToSpaces(BaseEditor *editor) {
    transformFragments(editor, [](const QString &t) {
        QString s = t;
        return s.replace(QLatin1Char('\t'), QLatin1String("    "));
    });
}

void spacesToTabs(BaseEditor *editor) {
    transformFragments(editor, [](const QString &t) {
        QString s = t;
        return s.replace(QLatin1String("    "), QLatin1String("\t"));
    });
}

} // namespace TextTools