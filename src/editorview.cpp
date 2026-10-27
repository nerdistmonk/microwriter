#include "editorview.h"

#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QScrollBar>
#include <QTextFrame>
#include <QTextCursor>
#include <QMouseEvent>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QUrl>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QImage>

EditorView::EditorView(QWidget *parent)
    : QTextEdit(parent),
      isPrintLayout(false),
      m_filePath(""),
      m_encodingName("UTF-8"),
      m_eolFormat(EolLf),
      m_headerText(""),
      m_footerText(""),
      m_selectedImagePos(-1),
      m_isResizingImage(false),
      m_dragWidth(0),
      m_dragHeight(0),
      m_imageAspectRatio(1.0)
{
    // Default document frame margins - Left set to 0 for flush start
    QTextFrameFormat fmt = document()->rootFrame()->frameFormat();
    fmt.setLeftMargin(0);
    fmt.setRightMargin(20);
    document()->rootFrame()->setFrameFormat(fmt);

    // Reset modification flag so initial frame setup doesn't make a blank document dirty
    document()->setModified(false);

    viewport()->setMouseTracking(true);
}

void EditorView::setDarkMode(bool dark) {
    Q_UNUSED(dark);
    // Background and text colors are updated by applyLayoutModeToEditor
}

QString EditorView::getContentForSave() {
    QString content;
    if (m_filePath.endsWith(".md")) {
        content = toMarkdown();

        for (QTextBlock blk = document()->begin(); blk.isValid(); blk = blk.next()) {
            for (QTextBlock::iterator it = blk.begin(); !it.atEnd(); ++it) {
                QTextFragment frag = it.fragment();
                if (frag.isValid() && frag.charFormat().isImageFormat()) {
                    QTextImageFormat ifmt = frag.charFormat().toImageFormat();
                    if (ifmt.width() > 0 && ifmt.height() > 0) {
                        QString target = QString("![image](%1)").arg(ifmt.name());
                        QString replacement = QString("<img src=\"%1\" width=\"%2\" height=\"%3\" />")
                            .arg(ifmt.name())
                            .arg((int)ifmt.width())
                            .arg((int)ifmt.height());
                        content.replace(target, replacement);
                    }
                }
            }
        }
    } else if (m_filePath.endsWith(".html") || m_filePath.endsWith(".htm")) {
        content = toHtml();
        content.replace(QRegularExpression("<!DOCTYPE[^>]*>", QRegularExpression::CaseInsensitiveOption), "<!DOCTYPE html>");
    } else {
        content = toPlainText();
    }
    return content;
}

void EditorView::setContent(const QString &content, const QString &fileName) {
    m_filePath = fileName;
    if (fileName.endsWith(".html") || fileName.endsWith(".htm")) {
        setHtml(content);
    } else if (fileName.endsWith(".md")) {
        setMarkdown(content);
    } else {
        setPlainText(content);
    }
    document()->setModified(false);
}

EditorStats EditorView::getStats() const {
    EditorStats s;
    QString txt = toPlainText();
    s.characters = txt.length();
    s.words = txt.trimmed().isEmpty() ? 0 : txt.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
    QTextCursor c = textCursor();
    s.line = c.blockNumber() + 1;
    s.column = c.columnNumber() + 1;
    s.encoding = m_encodingName;
    s.eol = m_eolFormat;
    return s;
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

QRect EditorView::currentImageRect() const {
    if (m_selectedImagePos < 0) return QRect();

    QTextCursor c(document());
    c.setPosition(m_selectedImagePos);

    QTextCursor check(c);
    check.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    QTextCharFormat fmt = check.charFormat();
    if (!fmt.isImageFormat()) return QRect();

    QTextImageFormat imgFmt = fmt.toImageFormat();
    QRect cr = cursorRect(c);

    int w = m_isResizingImage ? m_dragWidth : (int)imgFmt.width();
    int h = m_isResizingImage ? m_dragHeight : (int)imgFmt.height();

    if (w <= 0 || h <= 0) {
        QVariant res = document()->resource(QTextDocument::ImageResource, QUrl(imgFmt.name()));
        if (res.canConvert<QImage>()) {
            QImage img = res.value<QImage>();
            w = img.width();
            h = img.height();
        } else {
            w = 200;
            h = 150;
        }
    }

    return QRect(cr.left(), cr.top(), w, h);
}

QRect EditorView::cornerHandleRect() const {
    QRect imgRect = currentImageRect();
    if (imgRect.isEmpty()) return QRect();
    const int handleSize = 12;
    return QRect(imgRect.right() - handleSize / 2,
                 imgRect.bottom() - handleSize / 2,
                 handleSize, handleSize);
}

void EditorView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QTextCharFormat formatToKeep = currentCharFormat();

        // Block signals briefly so intermediate cursor change doesn't flash default font in toolbar
        bool oldBlocked = blockSignals(true);

        QTextEdit::keyPressEvent(event);

        setCurrentCharFormat(formatToKeep);

        blockSignals(oldBlocked);

        emit cursorPositionChanged();
        return;
    }

    QTextEdit::keyPressEvent(event);
}

QVariant EditorView::loadResource(int type, const QUrl &name) {
    if (type == QTextDocument::ImageResource) {
        // Check document resource cache first
        QVariant res = QTextEdit::loadResource(type, name);
        if (res.isValid() && !res.isNull()) {
            return res;
        }

        // If not cached, resolve relative to file location
        QString rawPath = name.toLocalFile();
        if (rawPath.isEmpty()) {
            rawPath = name.toString();
        }

        if (!m_filePath.isEmpty()) {
            QDir docDir = QFileInfo(m_filePath).dir();
            QString resolvedPath = docDir.filePath(rawPath);
            if (QFileInfo::exists(resolvedPath)) {
                QImage img(resolvedPath);
                if (!img.isNull()) {
                    document()->addResource(QTextDocument::ImageResource, name, img);
                    return img;
                }
            }
        }
    }
    return QTextEdit::loadResource(type, name);
}

void EditorView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_selectedImagePos >= 0 && cornerHandleRect().contains(event->pos())) {
            m_isResizingImage = true;
            QRect cur = currentImageRect();
            m_dragWidth = cur.width();
            m_dragHeight = cur.height();
            return;
        }

        QTextCursor clickedCursor = cursorForPosition(event->pos());
        int pos = clickedCursor.position();
        int imagePos = -1;

        if (pos < document()->characterCount()) {
            QTextCursor test(document());
            test.setPosition(pos);
            test.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            if (test.charFormat().isImageFormat()) {
                imagePos = pos;
            }
        }

        if (imagePos == -1 && pos > 0) {
            QTextCursor test(document());
            test.setPosition(pos - 1);
            test.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            if (test.charFormat().isImageFormat()) {
                imagePos = pos - 1;
            }
        }

        if (imagePos != -1) {
            QTextCursor imgCursor(document());
            imgCursor.setPosition(imagePos);
            imgCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            QTextImageFormat imgFmt = imgCursor.charFormat().toImageFormat();

            qreal w = imgFmt.width();
            qreal h = imgFmt.height();

            if (w <= 0 || h <= 0) {
                QVariant res = document()->resource(QTextDocument::ImageResource, QUrl(imgFmt.name()));
                if (res.canConvert<QImage>()) {
                    QImage img = res.value<QImage>();
                    w = img.width();
                    h = img.height();
                } else {
                    w = 200;
                    h = 150;
                }
            }

            m_selectedImagePos = imagePos;
            m_dragWidth = (int)w;
            m_dragHeight = (int)h;
            m_imageAspectRatio = (h > 0) ? (w / h) : 1.0;

            viewport()->update();
            return;
        } else {
            if (m_selectedImagePos != -1) {
                m_selectedImagePos = -1;
                viewport()->update();
            }
        }
    }

    QTextEdit::mousePressEvent(event);
}

void EditorView::mouseMoveEvent(QMouseEvent *event) {
    if (m_isResizingImage) {
        QRect imgRect = currentImageRect();
        int newWidth = event->pos().x() - imgRect.left();
        if (newWidth < 40) newWidth = 40;
        int newHeight = qRound(newWidth / m_imageAspectRatio);

        m_dragWidth = newWidth;
        m_dragHeight = newHeight;
        viewport()->update();
        return;
    }

    if (m_selectedImagePos >= 0 && cornerHandleRect().contains(event->pos())) {
        viewport()->setCursor(Qt::SizeFDiagCursor);
    } else {
        viewport()->setCursor(Qt::IBeamCursor);
    }

    QTextEdit::mouseMoveEvent(event);
}

void EditorView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_isResizingImage) {
        m_isResizingImage = false;

        if (m_selectedImagePos >= 0) {
            QTextCursor c(document());
            c.setPosition(m_selectedImagePos);
            c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

            if (c.charFormat().isImageFormat()) {
                QTextImageFormat fmt = c.charFormat().toImageFormat();
                fmt.setWidth(m_dragWidth);
                fmt.setHeight(m_dragHeight);
                c.setCharFormat(fmt);
                document()->setModified(true);
            }
        }

        viewport()->update();
        return;
    }

    QTextEdit::mouseReleaseEvent(event);
}

void EditorView::paintEvent(QPaintEvent *event) {
    QTextEdit::paintEvent(event);

    if (m_selectedImagePos != -1) {
        QRect imgRect = currentImageRect();
        if (!imgRect.isEmpty()) {
            QPainter painter(viewport());
            painter.setRenderHint(QPainter::Antialiasing, false);

            painter.setPen(QPen(QColor(0, 120, 215), 2, Qt::SolidLine));
            painter.drawRect(imgRect);

            QRect handle = cornerHandleRect();
            painter.fillRect(handle, QColor(0, 120, 215));
            painter.setPen(QColor(255, 255, 255));
            painter.drawRect(handle);
        }
    }

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
