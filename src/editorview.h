#ifndef EDITORVIEW_H
#define EDITORVIEW_H

#include <QTextEdit>
#include <QString>
#include <QStringList>
#include "types.h"

class QPaintEvent;
class QContextMenuEvent;

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

private:
    QString m_headerText;
    QString m_footerText;

    QStringList getAspellSuggestions(const QString &word);
};

#endif // EDITORVIEW_H
