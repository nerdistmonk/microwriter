#include "editorview.h"

#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QScrollBar>
#include <QTextFrame>
#include <QTextCursor>

EditorView::EditorView(QWidget *parent)
    : QTextEdit(parent),
      filePath(""),
      isPrintLayout(false),
      encodingName("UTF-8"),
      eolFormat(EolLf),
      m_headerText(""),
      m_footerText("")
{
    // Default document frame margins - Left set to 0 for flush start
    QTextFrameFormat fmt = document()->rootFrame()->frameFormat();
    fmt.setLeftMargin(0);
    fmt.setRightMargin(20);
    document()->rootFrame()->setFrameFormat(fmt);
}

void EditorView::setLeftMarginPixels(qreal pixels) {
    QTextFrameFormat fmt = document()->rootFrame()->frameFormat();
    fmt.setLeftMargin(pixels);
    document()->rootFrame()->setFrameFormat(fmt);
}

void EditorView::setRightMarginPixels(qreal pixels) {
    QTextFrameFormat fmt = document()->rootFrame()->frameFormat();
    fmt.setRightMargin(pixels);
    document()->rootFrame()->setFrameFormat(fmt);
}

qreal EditorView::leftMarginPixels() const {
    return document()->rootFrame()->frameFormat().leftMargin();
}

qreal EditorView::rightMarginPixels() const {
    return document()->rootFrame()->frameFormat().rightMargin();
}

void EditorView::paintEvent(QPaintEvent *event) {
    QTextEdit::paintEvent(event);

    if (isPrintLayout) {
        QPainter painter(viewport());
        painter.setPen(QPen(QColor(200, 50, 50), 2, Qt::DashLine));
        painter.setFont(QFont("Arial", 9, QFont::Bold));

        const int pageHeight = 1056; // 11 inches at 96 DPI
        const int scrollOffset = verticalScrollBar()->value();
        const int totalDocHeight = document()->size().height();

        for (int y = pageHeight; y < totalDocHeight; y += pageHeight) {
            int viewY = y - scrollOffset;
            if (viewY >= 0 && viewY <= viewport()->height()) {
                painter.drawLine(0, viewY, viewport()->width(), viewY);
                painter.setPen(QColor(150, 30, 30));
                painter.drawText(20, viewY - 6, QString("=== Page Break (Page %1) ===").arg((y / pageHeight) + 1));
                painter.setPen(QPen(QColor(200, 50, 50), 2, Qt::DashLine));
            }
        }
    }
}

void EditorView::contextMenuEvent(QContextMenuEvent *event) {
    QMenu *menu = createStandardContextMenu();

    QTextCursor cursor = cursorForPosition(event->pos());
    cursor.select(QTextCursor::WordUnderCursor);
    QString word = cursor.selectedText();

    if (!word.trimmed().isEmpty()) {
        QStringList suggestions = getAspellSuggestions(word);
        if (!suggestions.isEmpty()) {
            menu->addSeparator();
            QMenu *spellMenu = menu->addMenu("Spelling Suggestions");
            for (const QString &suggestion : suggestions) {
                QAction *action = spellMenu->addAction(suggestion);
                connect(action, &QAction::triggered, this, [=]() {
                    QTextCursor c = cursorForPosition(event->pos());
                    c.select(QTextCursor::WordUnderCursor);
                    c.insertText(suggestion);
                });
            }
        }
    }

    menu->exec(event->globalPos());
    delete menu;
}

QStringList EditorView::getAspellSuggestions(const QString &word) {
    QProcess process;
    process.start("aspell", QStringList() << "-a");
    if (!process.waitForStarted()) return QStringList();

    process.write(word.toUtf8() + "\n");
    process.closeWriteChannel();
    process.waitForFinished();

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList suggestions;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith('&')) {
            int colonIndex = line.indexOf(':');
            if (colonIndex != -1) {
                QString sugPart = line.mid(colonIndex + 1).trimmed();
                suggestions = sugPart.split(", ");
            }
        }
    }
    return suggestions;
}
