#include "codeeditorview.h"

#include <QPainter>
#include <QTextBlock>
#include <QFontDatabase>
#include <QTextCursor>
#include <QRegularExpression>

// --- BracketHighlighter ---

BracketHighlighter::BracketHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent), m_darkMode(false)
{
    m_colorsLight = {
        QColor(180, 80, 0),    // Level 1: Deep Orange
        QColor(0, 110, 180),   // Level 2: Blue
        QColor(140, 20, 140),  // Level 3: Purple
        QColor(20, 130, 20),   // Level 4: Green
        QColor(180, 20, 40)    // Level 5: Red
    };

    m_colorsDark = {
        QColor(255, 215, 0),   // Level 1: Gold
        QColor(138, 43, 226),  // Level 2: Blue Violet
        QColor(0, 191, 255),   // Level 3: Deep Sky Blue
        QColor(50, 205, 50),   // Level 4: Lime Green
        QColor(255, 105, 180)  // Level 5: Hot Pink
    };
}

void BracketHighlighter::setDarkMode(bool dark) {
    m_darkMode = dark;
    rehighlight();
}

void BracketHighlighter::highlightBlock(const QString &text) {
    const QVector<QColor> &palette = m_darkMode ? m_colorsDark : m_colorsLight;
    if (palette.isEmpty()) return;

    int depth = 0;
    for (int i = 0; i < text.length(); ++i) {
        QChar c = text.at(i);
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
