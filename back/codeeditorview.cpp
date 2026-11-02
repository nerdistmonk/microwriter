#include "codeeditorview.h"

#include <QPainter>
#include <QTextBlock>
#include <QFontDatabase>
#include <QTextCursor>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTextOption>
#include <QFileInfo>

// --- BracketHighlighter ---

BracketHighlighter::BracketHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent), m_darkMode(false)
{
    m_colorsLight = {
        QColor(180, 80, 0),   // Level 1: Deep Orange
        QColor(0, 110, 180),  // Level 2: Blue
        QColor(140, 20, 140), // Level 3: Purple
        QColor(20, 130, 20),  // Level 4: Green
        QColor(180, 20, 40)   // Level 5: Red
    };

    m_colorsDark = {
        QColor(255, 215, 0),  // Level 1: Gold
        QColor(138, 43, 226), // Level 2: Blue Violet
        QColor(0, 191, 255),  // Level 3: Deep Sky Blue
        QColor(50, 205, 50),  // Level 4: Lime Green
        QColor(255, 105, 180) // Level 5: Hot Pink
    };

    setupRules();
}

void BracketHighlighter::setupRules() {
    m_highlightingRules.clear();

    HighlightingRule rule;

    // 1. HTML / XML Tags (<tag> and </tag>)
    QTextCharFormat tagFormat;
    tagFormat.setForeground(m_darkMode ? QColor(86, 156, 214) : QColor(128, 0, 128));
    tagFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("</?[a-zA-Z0-9_-]+(\\s*|/?)>|/[a-zA-Z0-9_-]+"));
    rule.format = tagFormat;
    m_highlightingRules.append(rule);

    // 2. HTML Attributes (e.g., class=, href=, style=, src=)
    QTextCharFormat attrFormat;
    attrFormat.setForeground(m_darkMode ? QColor(156, 220, 254) : QColor(0, 100, 150));
    rule.pattern = QRegularExpression(QStringLiteral("\\b[a-zA-Z0-9_-]+(?=\\=)"));
    rule.format = attrFormat;
    m_highlightingRules.append(rule);

    // 3. Markdown Headers (# Header)
    QTextCharFormat mdHeaderFormat;
    mdHeaderFormat.setForeground(m_darkMode ? QColor(78, 201, 176) : QColor(0, 128, 128));
    mdHeaderFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("^\\s*#{1,6}\\s+.*$"));
    rule.format = mdHeaderFormat;
    m_highlightingRules.append(rule);

    // 4. Markdown Bullet / List Markers (- item or * item)
    QTextCharFormat mdListFormat;
    mdListFormat.setForeground(m_darkMode ? QColor(220, 220, 170) : QColor(160, 80, 0));
    mdListFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("^\\s*([*+-]|\\d+\\.)\\s+"));
    rule.format = mdListFormat;
    m_highlightingRules.append(rule);

    // 5. General Keywords (C/C++, Python, JS, HTML/Web)
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(m_darkMode ? QColor(86, 156, 214) : QColor(0, 0, 255));
    keywordFormat.setFontWeight(QFont::Bold);
    const QString keywordPatterns[] = {
        QStringLiteral("\\bchar\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bconst\\b"),
        QStringLiteral("\\bdouble\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bexplicit\\b"),
        QStringLiteral("\\bfriend\\b"), QStringLiteral("\\binline\\b"), QStringLiteral("\\bint\\b"),
        QStringLiteral("\\blong\\b"), QStringLiteral("\\bnamespace\\b"), QStringLiteral("\\boperator\\b"),
        QStringLiteral("\\bprivate\\b"), QStringLiteral("\\bprotected\\b"), QStringLiteral("\\bpublic\\b"),
        QStringLiteral("\\bshort\\b"), QStringLiteral("\\bsignals\\b"), QStringLiteral("\\bsigned\\b"),
        QStringLiteral("\\bslots\\b"), QStringLiteral("\\bstatic\\b"), QStringLiteral("\\bstruct\\b"),
        QStringLiteral("\\btemplate\\b"), QStringLiteral("\\btypedef\\b"), QStringLiteral("\\btypename\\b"),
        QStringLiteral("\\bunion\\b"), QStringLiteral("\\bunsigned\\b"), QStringLiteral("\\bvirtual\\b"),
        QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bvolatile\\b"), QStringLiteral("\\bbool\\b"),
        QStringLiteral("\\bauto\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bif\\b"),
        QStringLiteral("\\belse\\b"), QStringLiteral("\\bfor\\b"), QStringLiteral("\\bwhile\\b"),
        QStringLiteral("\\bdo\\b"), QStringLiteral("\\bswitch\\b"), QStringLiteral("\\bcase\\b"),
        QStringLiteral("\\bdefault\\b"), QStringLiteral("\\bbreak\\b"), QStringLiteral("\\bcontinue\\b"),
        QStringLiteral("\\bdef\\b"), QStringLiteral("\\bimport\\b"), QStringLiteral("\\bfrom\\b"),
        QStringLiteral("\\bfunction\\b"), QStringLiteral("\\blet\\b"), QStringLiteral("\\bvar\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        m_highlightingRules.append(rule);
    }

    // 6. Number literals
    QTextCharFormat numberFormat;
    numberFormat.setForeground(m_darkMode ? QColor(181, 206, 168) : QColor(9, 134, 88));
    rule.pattern = QRegularExpression(QStringLiteral("\\b(0x[0-9a-fA-F]+|\\d+(\\.\\d+)?)\\b"));
    rule.format = numberFormat;
    m_highlightingRules.append(rule);

    // 7. Single-line Strings
    QTextCharFormat stringFormat;
    stringFormat.setForeground(m_darkMode ? QColor(206, 145, 120) : QColor(163, 21, 21));
    rule.pattern = QRegularExpression(QStringLiteral("\".*?\"|'.*?'"));
    rule.format = stringFormat;
    m_highlightingRules.append(rule);

    // 8. Single-line comments (// and #)
    QTextCharFormat singleLineComment;
    singleLineComment.setForeground(m_darkMode ? QColor(106, 153, 85) : QColor(0, 128, 0));
    singleLineComment.setFontItalic(true);
    rule.pattern = QRegularExpression(QStringLiteral("//[^\n]*|#[^\n]*"));
    rule.format = singleLineComment;
    m_highlightingRules.append(rule);

    // 9. Trailing whitespace only
    QTextCharFormat trailingWhitespaceFormat;
    trailingWhitespaceFormat.setBackground(m_darkMode ? QColor(204, 34, 34) : QColor(255, 77, 77));
    rule.pattern = QRegularExpression(QStringLiteral("[ \\t]+$"));
    rule.format = trailingWhitespaceFormat;
    m_highlightingRules.append(rule);

    // 10. Multi-line comments (/* ... */ and <!-- ... -->)
    m_multiLineCommentFormat.setForeground(m_darkMode ? QColor(106, 153, 85) : QColor(0, 128, 0));
    m_multiLineCommentFormat.setFontItalic(true);
    m_commentStartRegex = QRegularExpression(QStringLiteral("/\\*|<!--"));
    m_commentEndRegex = QRegularExpression(QStringLiteral("\\*/|-->"));
}

void BracketHighlighter::setDarkMode(bool dark) {
    m_darkMode = dark;
    setupRules();
    rehighlight();
}

void BracketHighlighter::highlightBlock(const QString &text) {
    for (const HighlightingRule &rule : m_highlightingRules) {
        QRegularExpressionMatchIterator iter = rule.pattern.globalMatch(text);
        while (iter.hasNext()) {
            QRegularExpressionMatch match = iter.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    int prevState = previousBlockState();
    int depth = (prevState >= 0) ? (prevState & 0xFFFF) : 0;
    bool inComment = (prevState >= 0) ? ((prevState & 0x10000) != 0) : false;

    int startIndex = 0;
    if (!inComment) {
        startIndex = text.indexOf(m_commentStartRegex);
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch match = m_commentEndRegex.match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;
        if (endIndex == -1) {
            inComment = true;
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + match.capturedLength();
        }
        setFormat(startIndex, commentLength, m_multiLineCommentFormat);
        startIndex = inComment ? -1 : text.indexOf(m_commentStartRegex, startIndex + commentLength);
    }

    const QVector<QColor> &palette = m_darkMode ? m_colorsDark : m_colorsLight;
    if (!palette.isEmpty()) {
        bool inString = false;
        QChar stringQuote;

        for (int i = 0; i < text.length(); ++i) {
            QChar c = text.at(i);

            QTextCharFormat fmtAtPos = format(i);
            if (fmtAtPos.fontItalic()) {
                continue;
            }

            if (inString) {
                if (c == stringQuote && (i == 0 || text.at(i - 1) != '\\')) {
                    inString = false;
                }
                continue;
            } else if ((c == '"' || c == '\'') && (i == 0 || text.at(i - 1) != '\\')) {
                inString = true;
                stringQuote = c;
                continue;
            }

            if (c == '(' || c == '{' || c == '[') {
                int colorIndex = depth % palette.size();
                QTextCharFormat fmt;
                fmt.setForeground(palette[colorIndex]);
                fmt.setFontWeight(QFont::Bold);
                setFormat(i, 1, fmt);
                depth++;
            } else if (c == ')' || c == '}' || c == ']') {
                depth = qMax(0, depth - 1);
                int colorIndex = depth % palette.size();
                QTextCharFormat fmt;
                fmt.setForeground(palette[colorIndex]);
                fmt.setFontWeight(QFont::Bold);
                setFormat(i, 1, fmt);
            }
        }
    }

    int nextState = (depth & 0xFFFF) | (inComment ? 0x10000 : 0);
    setCurrentBlockState(nextState);
}

// --- LineNumberArea ---

LineNumberArea::LineNumberArea(CodeEditorView *editor)
    : QWidget(editor), m_editor(editor)
{
}

QSize LineNumberArea::sizeHint() const {
    return QSize(m_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event) {
    m_editor->lineNumberAreaPaintEvent(event);
}

void LineNumberArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_editor->lineNumberAreaSelectLine(event->pos().y());
    }
}

void LineNumberArea::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        m_editor->lineNumberAreaSelectLine(event->pos().y());
    }
}

// --- CodeEditorView ---

CodeEditorView::CodeEditorView(QWidget *parent)
    : QPlainTextEdit(parent),
      m_filePath(""),
      m_encodingName("UTF-8"),
      m_eolFormat(EolLf),
      m_darkMode(false)
{
    m_lineNumberArea = new LineNumberArea(this);

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(11);
    setFont(monoFont);

    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);

    m_highlighter = new BracketHighlighter(document());

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditorView::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditorView::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditorView::highlightCurrentLineAndBrackets);

    updateLineNumberAreaWidth(0);
    highlightCurrentLineAndBrackets();
    setDarkMode(false);
}

void CodeEditorView::setContent(const QString &content, const QString &fileName) {
    m_filePath = fileName;
    setPlainText(content);
    document()->setModified(false);
}

EditorStats CodeEditorView::getStats() const {
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

int CodeEditorView::lineNumberAreaWidth() {
    int digits = 1;
    int maxLines = qMax(1, blockCount());
    while (maxLines >= 10) {
        maxLines /= 10;
        digits++;
    }
    int space = 18 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditorView::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditorView::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy != 0) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void CodeEditorView::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditorView::setDarkMode(bool dark) {
    m_darkMode = dark;
    m_highlighter->setDarkMode(dark);

    if (dark) {
        setStyleSheet(
            "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
            "selection-background-color: #264f78; selection-color: #ffffff; }"
        );
    } else {
        setStyleSheet(
            "QPlainTextEdit { background-color: #ffffff; color: #1e1e1e; "
            "selection-background-color: #add6ff; selection-color: #000000; }"
        );
    }
    highlightCurrentLineAndBrackets();
    m_lineNumberArea->update();
}

void CodeEditorView::keyPressEvent(QKeyEvent *event) {
    if ((event->modifiers() & Qt::AltModifier) && !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
        if (event->key() == Qt::Key_Up) {
            handleMoveLine(true);
            return;
        } else if (event->key() == Qt::Key_Down) {
            handleMoveLine(false);
            return;
        }
    }

    if ((event->modifiers() & Qt::ControlModifier) && (event->key() == Qt::Key_Slash || event->key() == Qt::Key_Question)) {
        handleToggleComment();
        return;
    }

    if ((event->modifiers() & Qt::ControlModifier) && !(event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_D) {
        handleDuplicate();
        return;
    }

    if (event->key() == Qt::Key_Tab && !(event->modifiers() & Qt::ShiftModifier)) {
        if (textCursor().hasSelection()) {
            handleIndent();
            return;
        } else {
            insertPlainText("    ");
            return;
        }
    } else if (event->key() == Qt::Key_Backtab || (event->key() == Qt::Key_Tab && (event->modifiers() & Qt::ShiftModifier))) {
        handleUnindent();
        return;
    }

    if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier) {
        if (handleSmartBackspace()) {
            return;
        }
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        handleReturn();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

bool CodeEditorView::handleSmartBackspace() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) return false;

    int posInBlock = cursor.positionInBlock();
    if (posInBlock == 0) return false;

    QString text = cursor.block().text();
    QString before = text.left(posInBlock);

    bool allSpaces = true;
    for (QChar ch : before) {
        if (ch != ' ') {
            allSpaces = false;
            break;
        }
    }

    if (allSpaces && before.length() >= 2) {
        int spacesToDelete = posInBlock % 4;
        if (spacesToDelete == 0) spacesToDelete = 4;
        spacesToDelete = qMin(spacesToDelete, before.length());

        cursor.beginEditBlock();
        for (int i = 0; i < spacesToDelete; ++i) {
            cursor.deletePreviousChar();
        }
        cursor.endEditBlock();
        setTextCursor(cursor);
        return true;
    }

    return false;
}

void CodeEditorView::handleToggleComment() {
    QTextCursor cursor = textCursor();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    cursor.setPosition(start);
    int startBlock = cursor.blockNumber();
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    int endBlock = cursor.blockNumber();

    if (end > start && cursor.atBlockStart()) {
        endBlock--;
    }

    QString commentMarker = "// ";
    QString ext = QFileInfo(m_filePath).suffix().toLower();
    if (ext == "py" || ext == "sh" || ext == "bash" || ext == "ini" || ext == "cfg" || ext == "yaml" || ext == "yml" || ext == "cmake") {
        commentMarker = "# ";
    }

    bool allCommented = true;
    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock blk = document()->findBlockByNumber(i);
        if (!blk.isValid()) continue;
        QString trimmed = blk.text().trimmed();
        if (trimmed.isEmpty()) continue;

        if (commentMarker.startsWith("#")) {
            if (!trimmed.startsWith("#")) { allCommented = false; break; }
        } else {
            if (!trimmed.startsWith("//")) { allCommented = false; break; }
        }
    }

    cursor.beginEditBlock();
    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock blk = document()->findBlockByNumber(i);
        if (!blk.isValid()) continue;
        QString text = blk.text();

        if (allCommented) {
            int idx = text.indexOf(commentMarker);
            int len = commentMarker.length();
            if (idx == -1) {
                QString alt = commentMarker.trimmed();
                idx = text.indexOf(alt);
                len = alt.length();
            }
            if (idx != -1) {
                QTextCursor c(blk);
                c.setPosition(blk.position() + idx);
                c.setPosition(blk.position() + idx + len, QTextCursor::KeepAnchor);
                c.removeSelectedText();
            }
        } else {
            QTextCursor c(blk);
            int leadingSpaces = 0;
            while (leadingSpaces < text.length() && text.at(leadingSpaces).isSpace()) {
                leadingSpaces++;
            }
            c.setPosition(blk.position() + leadingSpaces);
            c.insertText(commentMarker);
        }
    }
    cursor.endEditBlock();
}

void CodeEditorView::handleDuplicate() {
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        QString text = cursor.selectedText();
        int end = cursor.selectionEnd();
        cursor.setPosition(end);
        cursor.insertText(text);
        setTextCursor(cursor);
    } else {
        QTextBlock blk = cursor.block();
        QString text = blk.text();
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertText("\n" + text);
        cursor.endEditBlock();
        setTextCursor(cursor);
    }
}

void CodeEditorView::handleMoveLine(bool moveUp) {
    QTextCursor cursor = textCursor();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    cursor.setPosition(start);
    int startBlock = cursor.blockNumber();
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    int endBlock = cursor.blockNumber();

    if (end > start && cursor.atBlockStart()) {
        endBlock--;
    }

    if (moveUp) {
        if (startBlock == 0) return;
        QTextBlock prevBlock = document()->findBlockByNumber(startBlock - 1);
        QString prevText = prevBlock.text();

        cursor.beginEditBlock();
        QTextCursor c(prevBlock);
        c.select(QTextCursor::BlockUnderCursor);
        c.removeSelectedText();

        QTextBlock targetBlock = document()->findBlockByNumber(endBlock - 1);
        QTextCursor insertCursor(targetBlock);
        insertCursor.movePosition(QTextCursor::EndOfBlock);
        insertCursor.insertText("\n" + prevText);
        cursor.endEditBlock();

        QTextBlock newStart = document()->findBlockByNumber(startBlock - 1);
        QTextBlock newEnd = document()->findBlockByNumber(endBlock - 1);
        QTextCursor newCursor(newStart);
        newCursor.movePosition(QTextCursor::StartOfBlock);
        newCursor.setPosition(newEnd.position() + newEnd.length() - 1, QTextCursor::KeepAnchor);
        setTextCursor(newCursor);
    } else {
        if (endBlock >= blockCount() - 1) return;
        QTextBlock nextBlock = document()->findBlockByNumber(endBlock + 1);
        QString nextText = nextBlock.text();

        cursor.beginEditBlock();
        QTextCursor c(nextBlock);
        c.select(QTextCursor::BlockUnderCursor);
        c.removeSelectedText();

        QTextBlock targetBlock = document()->findBlockByNumber(startBlock);
        QTextCursor insertCursor(targetBlock);
        insertCursor.movePosition(QTextCursor::StartOfBlock);
        insertCursor.insertText(nextText + "\n");
        cursor.endEditBlock();

        QTextBlock newStart = document()->findBlockByNumber(startBlock + 1);
        QTextBlock newEnd = document()->findBlockByNumber(endBlock + 1);
        QTextCursor newCursor(newStart);
        newCursor.movePosition(QTextCursor::StartOfBlock);
        newCursor.setPosition(newEnd.position() + newEnd.length() - 1, QTextCursor::KeepAnchor);
        setTextCursor(newCursor);
    }
}

void CodeEditorView::handleIndent() {
    QTextCursor cursor = textCursor();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    cursor.beginEditBlock();
    cursor.setPosition(start);
    int startBlock = cursor.blockNumber();
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    int endBlock = cursor.blockNumber();

    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock blk = document()->findBlockByNumber(i);
        if (blk.isValid()) {
            QTextCursor c(blk);
            c.insertText("    ");
        }
    }
    cursor.endEditBlock();
}

void CodeEditorView::handleUnindent() {
    QTextCursor cursor = textCursor();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    cursor.beginEditBlock();
    cursor.setPosition(start);
    int startBlock = cursor.blockNumber();
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    int endBlock = cursor.blockNumber();

    for (int i = startBlock; i <= endBlock; ++i) {
        QTextBlock blk = document()->findBlockByNumber(i);
        if (blk.isValid()) {
            QString t = blk.text();
            int spacesToRemove = 0;
            if (t.startsWith("    ")) spacesToRemove = 4;
            else if (t.startsWith('\t')) spacesToRemove = 1;
            else {
                while (spacesToRemove < t.length() && spacesToRemove < 4 && t.at(spacesToRemove) == ' ') {
                    spacesToRemove++;
                }
            }

            if (spacesToRemove > 0) {
                QTextCursor c(blk);
                c.setPosition(blk.position() + spacesToRemove, QTextCursor::KeepAnchor);
                c.removeSelectedText();
            }
        }
    }
    cursor.endEditBlock();
}

void CodeEditorView::handleReturn() {
    QTextCursor cursor = textCursor();
    QString currentLine = cursor.block().text();

    QString indent;
    for (QChar c : currentLine) {
        if (c.isSpace()) indent.append(c);
        else break;
    }

    QString trimmed = currentLine.trimmed();
    if (trimmed.endsWith('{') || trimmed.endsWith(':')) {
        indent.append("    ");
    }

    cursor.insertText("\n" + indent);
    ensureCursorVisible();
}

bool CodeEditorView::isBracket(QChar c) const {
    return c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']';
}

bool CodeEditorView::isOpeningBracket(QChar c) const {
    return c == '(' || c == '{' || c == '[';
}

QChar CodeEditorView::matchingBracket(QChar c) const {
    switch (c.toLatin1()) {
    case '(': return ')';
    case ')': return '(';
    case '{': return '}';
    case '}': return '{';
    case '[': return ']';
    case ']': return '[';
    default: return QChar();
    }
}

void CodeEditorView::matchBrackets(QList<QTextEdit::ExtraSelection> &selections) {
    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    QString docText = document()->toPlainText();
    if (docText.isEmpty()) return;

    int candidatePos = -1;
    if (pos < docText.length() && isBracket(docText.at(pos))) {
        candidatePos = pos;
    } else if (pos > 0 && isBracket(docText.at(pos - 1))) {
        candidatePos = pos - 1;
    }

    if (candidatePos == -1) return;

    QChar bracket = docText.at(candidatePos);
    QChar match = matchingBracket(bracket);
    bool open = isOpeningBracket(bracket);

    int targetPos = -1;
    int depth = 0;
    int step = open ? 1 : -1;
    int start = candidatePos + step;
    int end = open ? docText.length() : -1;

    for (int i = start; i != end; i += step) {
        QChar c = docText.at(i);
        if (c == bracket) {
            depth++;
        } else if (c == match) {
            if (depth == 0) {
                targetPos = i;
                break;
            }
            depth--;
        }
    }

    QColor matchBg = m_darkMode ? QColor(60, 90, 120) : QColor(190, 220, 255);
    QColor matchBorder = m_darkMode ? QColor(100, 160, 230) : QColor(0, 120, 215);

    auto highlightPos = [&](int p) {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(matchBg);
        sel.format.setProperty(QTextFormat::OutlinePen, QPen(matchBorder, 1));
        QTextCursor c(document());
        c.setPosition(p);
        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        sel.cursor = c;
        selections.append(sel);
    };

    highlightPos(candidatePos);
    if (targetPos != -1) {
        highlightPos(targetPos);
    }
}

void CodeEditorView::highlightCurrentLineAndBrackets() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection lineSel;
        QColor lineCol = m_darkMode ? QColor(40, 40, 40) : QColor(245, 245, 245);
        lineSel.format.setBackground(lineCol);
        lineSel.format.setProperty(QTextFormat::FullWidthSelection, true);
        lineSel.cursor = textCursor();
        lineSel.cursor.clearSelection();
        extraSelections.append(lineSel);
    }

    matchBrackets(extraSelections);
    setExtraSelections(extraSelections);
}

void CodeEditorView::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(m_lineNumberArea);
    QColor bgColor = m_darkMode ? QColor(30, 30, 30) : QColor(240, 240, 240);
    QColor numColor = m_darkMode ? QColor(120, 120, 120) : QColor(160, 160, 160);
    QColor curNumColor = m_darkMode ? QColor(255, 255, 255) : QColor(30, 30, 30);

    painter.fillRect(event->rect(), bgColor);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    int curLine = textCursor().blockNumber();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(blockNumber == curLine ? curNumColor : numColor);
            painter.setFont(font());
            painter.drawText(0, top, m_lineNumberArea->width() - 8, fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditorView::lineNumberAreaSelectLine(int y) {
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= y) {
        if (y >= top && y <= bottom) {
            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            setTextCursor(cursor);
            break;
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
}