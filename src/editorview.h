#ifndef EDITORVIEW_H
#define EDITORVIEW_H

#include <QTextEdit>
#include <QString>
#include <QStringList>
#include <QRect>
#include <QKeyEvent>
#include <QUrl>
#include <QVariant>
#include "types.h"
#include "baseeditor.h"

class QPaintEvent;
class QContextMenuEvent;
class QMouseEvent;

class EditorView : public QTextEdit, public BaseEditor {
    Q_OBJECT
public:
    explicit EditorView(QWidget *parent = nullptr);

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

    QString getContentForSave() override;
    void setContent(const QString &content, const QString &fileName) override;
    QString getRawPlainText() const override { return toPlainText(); }

    EditorStats getStats() const override;
    bool supportsRichFormatting() const override { return true; }

    void undo() override { QTextEdit::undo(); }
    void redo() override { QTextEdit::redo(); }
    void setDarkMode(bool dark) override;

    // View layout & margins
    bool isPrintLayout;
    QString headerText() const { return m_headerText; }
    void setHeaderText(const QString &text) { m_headerText = text; }

    QString footerText() const { return m_footerText; }
    void setFooterText(const QString &text) { m_footerText = text; }

    void setLeftMarginPixels(qreal pixels);
    void setRightMarginPixels(qreal pixels);
    qreal leftMarginPixels() const;
    qreal rightMarginPixels() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    QVariant loadResource(int type, const QUrl &name) override;

private:
    QString m_filePath;
    QString m_encodingName;
    EolFormat m_eolFormat;

    QString m_headerText;
    QString m_footerText;

    QStringList getAspellSuggestions(const QString &word);

    // Corner dragging state
    int m_selectedImagePos;
    bool m_isResizingImage;
    int m_dragWidth;
    int m_dragHeight;
    double m_imageAspectRatio;

    QRect currentImageRect() const;
    QRect cornerHandleRect() const;
};

#endif // EDITORVIEW_H
