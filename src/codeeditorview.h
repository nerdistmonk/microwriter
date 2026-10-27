#ifndef CODEEDITORVIEW_H
#define CODEEDITORVIEW_H

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QString>
#include <QList>
#include <QVector>
#include <QColor>
#include "types.h"
#include "baseeditor.h"

class QPaintEvent;
class QResizeEvent;
class CodeEditorView;

class BracketHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit BracketHighlighter(QTextDocument *parent = nullptr);
    void setDarkMode(bool dark);

protected:
    void highlightBlock(const QString &text) override;

private:
    bool m_darkMode;
    QVector<QColor> m_colorsLight;
    QVector<QColor> m_colorsDark;
};

class LineNumberArea : public QWidget {
    Q_OBJECT
public:
    explicit LineNumberArea(CodeEditorView *editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditorView *m_editor;
};

class CodeEditorView : public QPlainTextEdit, public BaseEditor {
    Q_OBJECT
public:
    explicit CodeEditorView(QWidget *parent = nullptr);

    // BaseEditor implementation
    QWidget* asWidget() override { return this; }
    QString filePath() const override { return m_filePath; }
    void setFilePath(const QString &path) override { m_filePath = path; }

    QString encodingName() const override { return m_encodingName; }
    void setEncodingName(const QString &encoding) override { m_encodingName = encoding; }

    EolFormat eolFormat() const override { return m_eolFormat; }
    void setEolFormat(EolFormat format) override { m_eolFormat = format; }

    bool isModified() const override { return document()->isModified(); }
    void setModified(bool modified) override { document()->setModified(modified); }

    QString getContentForSave() override { return toPlainText(); }
    void setContent(const QString &content, const QString &fileName) override;
    QString getRawPlainText() const override { return toPlainText(); }

    EditorStats getStats() const override;
    bool supportsRichFormatting() const override { return false; }

    void undo() override { QPlainTextEdit::undo(); }
    void redo() override { QPlainTextEdit::redo(); }
    void setDarkMode(bool dark) override;

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLineAndBrackets();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QString m_filePath;
    QString m_encodingName;
    EolFormat m_eolFormat;

    QWidget *m_lineNumberArea;
    BracketHighlighter *m_highlighter;
    bool m_darkMode;

    void matchBrackets(QList<QTextEdit::ExtraSelection> &selections);
    bool isBracket(QChar c) const;
    bool isOpeningBracket(QChar c) const;
    QChar matchingBracket(QChar c) const;
};

#endif // CODEEDITORVIEW_H
