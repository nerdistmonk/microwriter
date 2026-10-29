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
      m_layoutMode(NormalLayout),
      isPrintLayout(false),
      m_filePath(""),
      m_encodingName("UTF-8"),
      m_eolFormat(EolLf),
      m_headerText(""),
      m_footerText(""),
      m_rawMarkdownSource(""),
      m_selectedImagePos(-1),
      m_isResizingImage(false),
      m_dragWidth(0),
      m_dragHeight(0),
      m_imageAspectRatio(1.0)
{
    QTextFrameFormat fmt = document()->rootFrame()->frameFormat();
    fmt.setLeftMargin(0);
    fmt.setRightMargin(20);
    document()->rootFrame()->setFrameFormat(fmt);

    document()->setModified(false);
    viewport()->setMouseTracking(true);
}

void EditorView::setDarkMode(bool dark) {
    Q_UNUSED(dark);
}

QString EditorView::getContentForSave() {
    QString content;
    QDir docDir = m_filePath.isEmpty() ? QDir::current() : QFileInfo(m_filePath).dir();

    if (m_filePath.endsWith(".md", Qt::CaseInsensitive)) {
        if (!document()->isModified() && !m_rawMarkdownSource.isEmpty()) {
            return m_rawMarkdownSource;
        }

        content = toMarkdown();

        for (QTextBlock blk = document()->begin(); blk.isValid(); blk = blk.next()) {
            for (QTextBlock::iterator it = blk.begin(); !it.atEnd(); ++it) {
                QTextFragment frag = it.fragment();
                if (frag.isValid() && frag.charFormat().isImageFormat()) {
                    QTextImageFormat ifmt = frag.charFormat().toImageFormat();
                    QString rawName = ifmt.name();
                    QString relPath = docDir.relativeFilePath(rawName);
                    relPath.replace('\\', '/');

                    QRegularExpression imgRe("!\\[.*?\\]\\(" + QRegularExpression::escape(rawName) + "\\)");
                    QString replacement;
                    if (ifmt.width() > 0 && ifmt.height() > 0) {
                        replacement = QString("<img src=\"%1\" width=\"%2\" height=\"%3\" />")
                                          .arg(relPath)
                                          .arg((int)ifmt.width())
                                          .arg((int)ifmt.height());
                    } else {
                        replacement = QString("![](%1)").arg(relPath);
                    }
                    content.replace(imgRe, replacement);
                }
            }
        }

        content.replace(QRegularExpression("(</img>|<img[^>]+/>)\\s*\\n\\s*(<img)"), "\\1 \\2");
        m_rawMarkdownSource = content;
        return content;
    } else if (m_filePath.endsWith(".html", Qt::CaseInsensitive) || m_filePath.endsWith(".htm", Qt::CaseInsensitive) || m_filePath.isEmpty()) {
        content = toHtml();
        content.replace(QRegularExpression("<!DOCTYPE[^>]*>", QRegularExpression::CaseInsensitiveOption), "<!DOCTYPE html>");

        QRegularExpression imgTagRe("<img[^>]+src=\"([^\"]+)\"[^>]*>");
        QRegularExpressionMatchIterator iter = imgTagRe.globalMatch(content);
        QMap<QString, QString> replacements;

        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            QString src = match.captured(1);
            if (QDir::isAbsolutePath(src) || src.startsWith("file:")) {
                QString localFile = src.startsWith("file:") ? QUrl(src).toLocalFile() : src;
                QString relPath = docDir.relativeFilePath(localFile);
                relPath.replace('\\', '/');
                replacements[src] = relPath;
            }
        }

        for (auto it = replacements.constBegin(); it != replacements.constEnd(); ++it) {
            content.replace(QString("src=\"%1\"").arg(it.key()), QString("src=\"%1\"").arg(it.value()));
        }
    } else {
        content = toPlainText();
    }
    return content;
}

void EditorView::setContent(const QString &content, const QString &fileName) {
    m_filePath = fileName;
    m_rawMarkdownSource.clear();

    if (fileName.endsWith(".md", Qt::CaseInsensitive)) {
        m_rawMarkdownSource = content;
        setMarkdown(content);
    } else if (fileName.endsWith(".html", Qt::CaseInsensitive) || fileName.endsWith(".htm", Qt::CaseInsensitive)) {
        setHtml(content);
    } else if (fileName.isEmpty()) {
        if (content.contains("<html", Qt::CaseInsensitive) || content.contains("<!DOCTYPE", Qt::CaseInsensitive) || content.contains("<p>", Qt::CaseInsensitive)) {
            setHtml(content);
        } else {
            setPlainText(content);
        }
    } else {
        setPlainText(content);
    }

    m_selectedImagePos = -1;
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
    if (m_selectedImagePos != -1) {
        m_selectedImagePos = -1;
        viewport()->update();
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QTextCharFormat formatToKeep = currentCharFormat();

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
        QVariant res = QTextEdit::loadResource(type, name);
        if (res.isValid() && !res.isNull()) {
            return res;
        }

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
        int newHeight = (int)(newWidth / m_imageAspectRatio);
        if (newHeight < 40) newHeight = 40;

        m_dragWidth = newWidth;
        m_dragHeight = newHeight;
        viewport()->update();
        return;
    }

    QTextEdit::mouseMoveEvent(event);
}

void EditorView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_isResizingImage && m_selectedImagePos >= 0) {
        m_isResizingImage = false;

        QTextCursor c(document());
        c.setPosition(m_selectedImagePos);
        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

        QTextCharFormat fmt = c.charFormat();
        if (fmt.isImageFormat()) {
            QTextImageFormat imgFmt = fmt.toImageFormat();
            imgFmt.setWidth(m_dragWidth);
            imgFmt.setHeight(m_dragHeight);
            c.setCharFormat(imgFmt);
        }

        m_selectedImagePos = -1;
        viewport()->update();
        return;
    }

    QTextEdit::mouseReleaseEvent(event);
}

void EditorView::contextMenuEvent(QContextMenuEvent *event) {
    QMenu *menu = createStandardContextMenu();

    QTextCursor cursor = cursorForPosition(event->pos());
    cursor.select(QTextCursor::WordUnderCursor);
    QString word = cursor.selectedText().trimmed();

    if (!word.isEmpty()) {
        QStringList suggestions = getAspellSuggestions(word);
        if (!suggestions.isEmpty()) {
            QMenu *spellMenu = new QMenu(tr("Spelling Suggestions"), menu);
            for (const QString &s : suggestions) {
                QAction *act = spellMenu->addAction(s);
                connect(act, &QAction::triggered, this, [this, cursor, s]() {
                    QTextCursor c = cursor;
                    c.insertText(s);
                });
            }
            menu->insertMenu(menu->actions().first(), spellMenu);
            menu->insertSeparator(menu->actions().at(1));
        }
    }

    menu->exec(event->globalPos());
    delete menu;
}

void EditorView::paintEvent(QPaintEvent *event) {
    QTextEdit::paintEvent(event);

    if (m_selectedImagePos >= 0) {
        QRect imgRect = currentImageRect();
        if (!imgRect.isEmpty()) {
            QPainter painter(viewport());
            painter.setRenderHint(QPainter::Antialiasing, false);

            QPen pen(QColor(0, 120, 215), 1, Qt::DashLine);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(imgRect);

            QRect handle = cornerHandleRect();
            painter.setPen(QColor(0, 120, 215));
            painter.setBrush(QColor(255, 255, 255));
            painter.drawRect(handle);
        }
    }
}

QStringList EditorView::getAspellSuggestions(const QString &word) {
    QProcess aspell;
    aspell.start("aspell", QStringList() << "-a");
    if (!aspell.waitForStarted(1000)) return QStringList();

    aspell.write(word.toUtf8() + "\n");
    aspell.closeWriteChannel();
    if (!aspell.waitForFinished(1000)) return QStringList();

    QString out = QString::fromUtf8(aspell.readAllStandardOutput());
    QStringList lines = out.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        if (line.startsWith("&")) {
            int colonIdx = line.indexOf(':');
            if (colonIdx != -1) {
                QString listPart = line.mid(colonIdx + 1).trimmed();
                return listPart.split(", ", Qt::SkipEmptyParts);
            }
        }
    }
    return QStringList();
}