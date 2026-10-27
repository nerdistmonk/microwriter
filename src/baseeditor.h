#ifndef BASEEDITOR_H
#define BASEEDITOR_H

#include <QWidget>
#include <QString>
#include "types.h"

class BaseEditor {
public:
    virtual ~BaseEditor() = default;

    virtual QWidget* asWidget() = 0;
    virtual QString filePath() const = 0;
    virtual void setFilePath(const QString &path) = 0;

    virtual QString encodingName() const = 0;
    virtual void setEncodingName(const QString &encoding) = 0;

    virtual EolFormat eolFormat() const = 0;
    virtual void setEolFormat(EolFormat format) = 0;

    virtual bool isModified() const = 0;
    virtual void setModified(bool modified) = 0;

    virtual QString getContentForSave() = 0;
    virtual void setContent(const QString &content, const QString &fileName) = 0;
    virtual QString getRawPlainText() const = 0;

    virtual EditorStats getStats() const = 0;
    virtual bool supportsRichFormatting() const = 0;

    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual void setDarkMode(bool dark) = 0;
};

#endif // BASEEDITOR_H
