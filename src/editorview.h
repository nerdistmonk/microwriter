#ifndef EDITORVIEW_H
#define EDITORVIEW_H

#include <QTextEdit>
#include <QString>
#include <QStringList>
#include <QRect>
#include "types.h"

class QPaintEvent;
class QContextMenuEvent;
class QMouseEvent;

class EditorView : public QTextEdit {
    Q_OBJECT
public:
    explicit EditorView(QWidget *parent = nullptr);

    QString filePath;
    bool isPrintLayout;
    QString encodingName;
    EolFormat eolFormat;

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

private:
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
