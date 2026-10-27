#include "documentexporter.h"
#include "editorview.h"

#include <QPrinter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPainter>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QPageSize>
#include <QPageLayout>
#include <QAbstractTextDocumentLayout>
#include <QTextDocumentWriter>

namespace DocumentExporter {

void printDocument(EditorView *editor, QWidget *parent) {
    if (!editor) return;

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, parent);
    if (dialog.exec() == QDialog::Accepted) {
        editor->print(&printer);
    }
}

void printPreview(EditorView *editor, QWidget *parent) {
    if (!editor) return;

    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, parent);
    QObject::connect(&preview, &QPrintPreviewDialog::paintRequested, parent, [editor](QPrinter *prn) {
        editor->print(prn);
    });
    preview.exec();
}

void exportPdf(EditorView *editor, QWidget *parent) {
    if (!editor) return;

    QString defaultPath = "untitled.pdf";
    if (!editor->filePath.isEmpty()) {
        QFileInfo fi(editor->filePath);
        defaultPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".pdf";
    }

    QString fileName = QFileDialog::getSaveFileName(
        parent,
        "Export PDF",
        defaultPath,
        "PDF Files (*.pdf)",
        nullptr,
        QFileDialog::DontUseNativeDialog
    );
    if (fileName.isEmpty()) return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) {
        fileName += ".pdf";
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize(QPageSize::Letter));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    if (editor->headerText().isEmpty() && editor->footerText().isEmpty()) {
        QTextDocument *doc = editor->document()->clone();

        QTextCursor cursor(doc);
        cursor.select(QTextCursor::Document);
        QTextCharFormat fmt = cursor.charFormat();
        if (fmt.foreground().color().lightness() > 150) {
            fmt.setForeground(Qt::black);
            cursor.mergeCharFormat(fmt);
        }

        doc->print(&printer);
        delete doc;
    } else {
        QPainter painter;
        if (!painter.begin(&printer)) {
            QMessageBox::critical(parent, "Export PDF Error", "Could not initialize PDF painter engine.");
            return;
        }

        QTextDocument *doc = editor->document()->clone();
        QRectF pageRect = printer.pageLayout().paintRectPixels(printer.resolution());
        qreal dpiScale = printer.resolution() / 96.0;

        qreal headerHeight = editor->headerText().isEmpty() ? 0 : (40.0 * dpiScale);
        qreal footerHeight = editor->footerText().isEmpty() ? 0 : (40.0 * dpiScale);
        qreal bodyHeight = pageRect.height() - headerHeight - footerHeight;

        doc->setPageSize(QSizeF(pageRect.width() / dpiScale, bodyHeight / dpiScale));
        int pageCount = doc->pageCount();

        for (int page = 0; page < pageCount; ++page) {
            if (page > 0) {
                printer.newPage();
            }

            if (!editor->headerText().isEmpty()) {
                painter.setFont(QFont("Arial", 10));
                painter.setPen(Qt::black);
                painter.drawText(QRectF(0, 0, pageRect.width(), headerHeight), Qt::AlignCenter, editor->headerText());
            }

            if (!editor->footerText().isEmpty()) {
                painter.setFont(QFont("Arial", 10));
                painter.setPen(Qt::black);
                painter.drawText(QRectF(0, pageRect.height() - footerHeight, pageRect.width(), footerHeight), Qt::AlignCenter, editor->footerText());
            }

            painter.save();
            painter.translate(0, headerHeight);
            painter.scale(dpiScale, dpiScale);
            painter.translate(0, - (page * (bodyHeight / dpiScale)));

            QAbstractTextDocumentLayout::PaintContext ctx;
            ctx.clip = QRectF(0, page * (bodyHeight / dpiScale), pageRect.width() / dpiScale, bodyHeight / dpiScale);
            ctx.palette.setColor(QPalette::Text, Qt::black);

            doc->documentLayout()->draw(&painter, ctx);
            painter.restore();
        }

        painter.end();
        delete doc;
    }

    QFileInfo checkFile(fileName);
    if (checkFile.exists() && checkFile.size() > 0) {
        QMessageBox::information(parent, "Export PDF", "PDF exported successfully:\n" + fileName);
    } else {
        QMessageBox::critical(parent, "Export PDF Error", "Could not generate PDF file.");
    }
}

void exportOdt(EditorView *editor, QWidget *parent) {
    if (!editor) return;

    QString defaultPath = "untitled.odt";
    if (!editor->filePath.isEmpty()) {
        QFileInfo fi(editor->filePath);
        defaultPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".odt";
    }

    QString fileName = QFileDialog::getSaveFileName(
        parent,
        "Export as ODT",
        defaultPath,
        "OpenDocument Text (*.odt)",
        nullptr,
        QFileDialog::DontUseNativeDialog
    );
    if (fileName.isEmpty()) return;

    if (!fileName.endsWith(".odt", Qt::CaseInsensitive)) {
        fileName += ".odt";
    }

    QTextDocument *docCopy = editor->document()->clone();
    QTextDocumentWriter writer(fileName, "ODF");
    bool success = writer.write(docCopy);
    delete docCopy;

    QFileInfo checkFile(fileName);
    if (success && checkFile.exists() && checkFile.size() > 0) {
        QMessageBox::information(parent, "Export ODT", "ODT file exported successfully:\n" + fileName);
    } else {
        QMessageBox::critical(parent, "Export ODT Error", "Failed to write ODT document format.");
    }
}

} // namespace DocumentExporter
