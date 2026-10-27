#include "documentformatter.h"
#include "editorview.h"
#include "dialogs.h"

#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextListFormat>
#include <QTextTableFormat>
#include <QColorDialog>
#include <QFileDialog>
#include <QDateTime>
#include <QImage>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

namespace DocumentFormatter {

static void mergeCharFormat(EditorView *editor, const QTextCharFormat &format) {
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(format);
    } else {
        editor->mergeCurrentCharFormat(format);
    }
}

void toggleBold(EditorView *editor) {
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontWeight(editor->currentCharFormat().fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
    mergeCharFormat(editor, fmt);
}

void toggleItalic(EditorView *editor) {
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontItalic(!editor->currentCharFormat().fontItalic());
    mergeCharFormat(editor, fmt);
}

void toggleUnderline(EditorView *editor) {
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontUnderline(!editor->currentCharFormat().fontUnderline());
    mergeCharFormat(editor, fmt);
}

void toggleStrikethrough(EditorView *editor) {
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontStrikeOut(!editor->currentCharFormat().fontStrikeOut());
    mergeCharFormat(editor, fmt);
}

void toggleSuperscript(EditorView *editor) {
    if (!editor) return;
    QTextCharFormat fmt;
    int align = editor->currentCharFormat().verticalAlignment();
    fmt.setVerticalAlignment(align == QTextCharFormat::AlignSuperScript ? QTextCharFormat::AlignNormal : QTextCharFormat::AlignSuperScript);
    mergeCharFormat(editor, fmt);
}

void toggleSubscript(EditorView *editor) {
    if (!editor) return;
    QTextCharFormat fmt;
    int align = editor->currentCharFormat().verticalAlignment();
    fmt.setVerticalAlignment(align == QTextCharFormat::AlignSubScript ? QTextCharFormat::AlignNormal : QTextCharFormat::AlignSubScript);
    mergeCharFormat(editor, fmt);
}

void applyTextColor(EditorView *editor, QWidget *parent) {
    if (!editor) return;
    QColor col = QColorDialog::getColor(editor->textColor(), parent, "Choose Text Color");
    if (col.isValid()) {
        QTextCharFormat fmt;
        fmt.setForeground(col);
        mergeCharFormat(editor, fmt);
    }
}

void applyTextShading(EditorView *editor, QWidget *parent) {
    if (!editor) return;
    QColor col = QColorDialog::getColor(Qt::yellow, parent, "Choose Text Shading Color");
    if (col.isValid()) {
        QTextCharFormat fmt;
        fmt.setBackground(col);
        mergeCharFormat(editor, fmt);
    }
}

void applyFontFamily(EditorView *editor, const QFont &font) {
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontFamilies(QStringList{font.family()});

    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(fmt);
    } else {
        editor->mergeCurrentCharFormat(fmt);
    }

    if (editor->toPlainText().isEmpty()) {
        QFont docFont = editor->document()->defaultFont();
        docFont.setFamily(font.family());
        editor->document()->setDefaultFont(docFont);
    }
}

void applyFontSize(EditorView *editor, int size) {
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontPointSize(size);

    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(fmt);
    } else {
        editor->mergeCurrentCharFormat(fmt);
    }

    if (editor->toPlainText().isEmpty()) {
        QFont docFont = editor->document()->defaultFont();
        docFont.setPointSize(size);
        editor->document()->setDefaultFont(docFont);
    }
}

void setAlignment(EditorView *editor, Qt::Alignment align) {
    if (editor) editor->setAlignment(align);
}

void toggleDoubleSpacing(EditorView *editor, bool doubleSpace) {
    if (!editor) return;
    QTextCursor cursor(editor->document());
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);

    do {
        QTextBlockFormat fmt = cursor.blockFormat();
        fmt.setLineHeight(doubleSpace ? 200.0 : 100.0, QTextBlockFormat::ProportionalHeight);
        cursor.setBlockFormat(fmt);
    } while (cursor.movePosition(QTextCursor::NextBlock));

    cursor.endEditBlock();
    editor->document()->setModified(true);
}

void insertBulletList(EditorView *editor) {
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    QTextListFormat listFormat;
    listFormat.setStyle(QTextListFormat::ListDisc);
    cursor.createList(listFormat);
}

void insertNumberedList(EditorView *editor) {
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    QTextListFormat listFormat;
    listFormat.setStyle(QTextListFormat::ListDecimal);
    cursor.createList(listFormat);
}

void insertHorizontalLine(EditorView *editor) {
    if (!editor) return;
    editor->textCursor().insertHtml("<hr>");
}

void insertBlockquote(EditorView *editor) {
    if (!editor) return;
    editor->textCursor().insertHtml("<blockquote style=\"border-left: 3px solid #007acc; margin: 10px 0; padding-left: 10px; color: #555;\"><p>Type blockquote text here...</p></blockquote>");
}

void insertTable(EditorView *editor, QWidget *parent) {
    if (!editor) return;
    TableDialog dlg(parent);
    if (dlg.exec() == QDialog::Accepted) {
        QTextCursor cursor = editor->textCursor();
        QTextTableFormat tableFormat;
        tableFormat.setAlignment(Qt::AlignHCenter);
        tableFormat.setCellPadding(5);
        tableFormat.setCellSpacing(0);
        tableFormat.setBorder(1);
        tableFormat.setBorderBrush(Qt::darkGray);
        cursor.insertTable(dlg.rows(), dlg.cols(), tableFormat);
    }
}

void insertDateTime(EditorView *editor, QWidget *parent) {
    if (!editor) return;
    DateTimeDialog dlg(parent);
    if (dlg.exec() == QDialog::Accepted) {
        editor->textCursor().insertText(QDateTime::currentDateTime().toString(dlg.selectedFormatString()));
    }
}

void insertLink(EditorView *editor, QWidget *parent) {
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    QString selectedText = cursor.hasSelection() ? cursor.selectedText() : "";

    LinkDialog dlg(selectedText, parent);
    if (dlg.exec() == QDialog::Accepted) {
        QString url = dlg.url();
        QString text = dlg.linkText().isEmpty() ? url : dlg.linkText();
        if (!url.isEmpty()) {
            cursor.insertHtml(QString("<a href=\"%1\">%2</a>").arg(url, text));
        }
    }
}

void insertSymbol(EditorView *editor, QWidget *parent) {
    if (!editor) return;
    SymbolDialog dlg(parent);
    if (dlg.exec() == QDialog::Accepted && !dlg.getSelectedSymbol().isEmpty()) {
        editor->textCursor().insertText(dlg.getSelectedSymbol());
    }
}

void insertImage(EditorView *editor, QWidget *parent) {
    if (!editor) return;
    QString fileName = QFileDialog::getOpenFileName(
        parent, "Insert Image", "",
        "Images (*.png *.jpg *.jpeg *.bmp *.gif)",
        nullptr, QFileDialog::DontUseNativeDialog);
        
    if (!fileName.isEmpty()) {
        QImage image(fileName);
        if (!image.isNull()) {
            // Determine relative path if the document already has a file path,
            // otherwise use fileName or relative file name
            QString imageRef = fileName;
            if (!editor->filePath().isEmpty()) {
                QDir docDir = QFileInfo(editor->filePath()).dir();
                imageRef = docDir.relativeFilePath(fileName);
            } else {
                imageRef = QFileInfo(fileName).fileName();
            }

            // Register the image with the document under imageRef
            // so Qt can render it in the editor viewport
            editor->document()->addResource(QTextDocument::ImageResource, QUrl(imageRef), image);

            // Insert via format pointing to that clean reference name
            QTextImageFormat format;
            format.setName(imageRef);
            editor->textCursor().insertImage(format);
        }
    }
}

void configureHeaderFooter(EditorView *editor, QWidget *parent) {
    if (!editor) return;
    HeaderFooterDialog dlg(editor->headerText(), editor->footerText(), parent);
    if (dlg.exec() == QDialog::Accepted) {
        editor->setHeaderText(dlg.getHeader());
        editor->setFooterText(dlg.getFooter());
        editor->document()->setModified(true);
        editor->viewport()->update();
    }
}

} // namespace DocumentFormatter
