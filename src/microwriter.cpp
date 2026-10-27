#include "microwriter.h"
#include "editorview.h"
#include "ruler.h"
#include "dialogs.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QStatusBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QTextStream>
#include <QFontComboBox>
#include <QSpinBox>
#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QStringConverter>
#include <QRegularExpression>
#include <QPrinter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QPainter>
#include <QAbstractTextDocumentLayout>
#include <QTextDocumentWriter>
#include <QTextListFormat>
#include <QTextTableFormat>
#include <algorithm>

MicroWriter::MicroWriter(QWidget *parent)
    : QMainWindow(parent),
      ruler(nullptr),
      tabWidget(nullptr),
      statusLabel(nullptr),
      fontCombo(nullptr),
      sizeSpin(nullptr),
      currentLayoutMode(NormalLayout),
      darkModeActive(false),
      darkAct(nullptr)
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    ruler = new Ruler(this);
    mainLayout->addWidget(ruler);

    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);
    tabWidget->setMovable(true);
    mainLayout->addWidget(tabWidget);

    setCentralWidget(centralWidget);

    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MicroWriter::closeTabRequested);
    connect(tabWidget, &QTabWidget::currentChanged, this, [=](int index){
        updateWindowTitleAndStats();
        EditorView *ed = qobject_cast<EditorView*>(tabWidget->widget(index));
        ruler->setEditor(ed);
        applyLayoutModeToEditor(ed, currentLayoutMode);
        if (ed) {
            disconnect(ed, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateStats);
            disconnect(ed, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateFormatToolbarState);
            connect(ed, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateStats);
            connect(ed, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateFormatToolbarState);
            updateFormatToolbarState();
        }
    });

    createMenus();
    loadToolbarSettings();
    createStatusBar();

    resize(1100, 750);
    newFile();

    QSettings settings("MicroWriterOrg", "MicroWriterApp");
    bool savedDarkMode = settings.value("darkMode", false).toBool();
    if (savedDarkMode) {
        darkAct->setChecked(true);
        toggleDarkMode(true);
    }
}

void MicroWriter::openFilePath(const QString &fileName) {
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        EditorView *editor = new EditorView(this);

        QByteArray rawData = file.readAll();
        if (rawData.contains("\r\n")) editor->eolFormat = EolCrlf;
        else if (rawData.contains('\r') && !rawData.contains('\n')) editor->eolFormat = EolCr;
        else editor->eolFormat = EolLf;

        QTextStream in(&rawData);
        in.setEncoding(QStringConverter::encodingForName(editor->encodingName.toUtf8()).value_or(QStringConverter::Utf8));
        QString content = in.readAll();

        connect(editor, &QTextEdit::textChanged, this, &MicroWriter::updateStats);
        connect(editor, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateStats);
        connect(editor, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateFormatToolbarState);

        connect(editor->document(), &QTextDocument::modificationChanged, this, [=](bool changed){
            int idx = tabWidget->indexOf(editor);
            if (idx != -1) {
                QFileInfo fi(editor->filePath);
                QString name = editor->filePath.isEmpty() ? "Untitled" : fi.fileName();
                if (changed) name += " *";
                tabWidget->setTabText(idx, name);
            }
        });

        if (fileName.endsWith(".html") || fileName.endsWith(".htm")) {
            editor->setHtml(content);
        } else if (fileName.endsWith(".md")) {
            editor->setMarkdown(content);
        } else {
            editor->setPlainText(content);
        }

        editor->filePath = fileName;
        editor->document()->setModified(false);

        QFileInfo fileInfo(fileName);

        if (tabWidget->count() == 1) {
            EditorView *firstEd = qobject_cast<EditorView*>(tabWidget->widget(0));
            if (firstEd && firstEd->filePath.isEmpty() && !firstEd->document()->isModified() && firstEd->toPlainText().isEmpty()) {
                tabWidget->removeTab(0);
                delete firstEd;
            }
        }

        int index = tabWidget->addTab(editor, fileInfo.fileName());
        tabWidget->setCurrentIndex(index);
        ruler->setEditor(editor);
        applyLayoutModeToEditor(editor, currentLayoutMode);
        updateFormatToolbarState();
    }
}

void MicroWriter::saveToolbarSettings() {
    QSettings settings("MicroWriterOrg", "MicroWriterApp");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    settings.beginWriteArray("Toolbars");
    for (int i = 0; i < toolbars.size(); ++i) {
        settings.setArrayIndex(i);
        QToolBar *tb = toolbars.at(i);
        settings.setValue("name", tb->windowTitle());

        QStringList actionKeys;
        for (QAction *act : tb->actions()) {
            if (!act->objectName().isEmpty()) {
                actionKeys.append(act->objectName());
            }
        }
        settings.setValue("actions", actionKeys);
    }
    settings.endArray();
}

void MicroWriter::loadToolbarSettings() {
    QSettings settings("MicroWriterOrg", "MicroWriterApp");

    int count = settings.beginReadArray("Toolbars");
    if (count == 0) {
        settings.endArray();
        createDefaultToolBar();
        return;
    }

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        QString name = settings.value("name").toString();
        QStringList actionKeys = settings.value("actions").toStringList();

        QToolBar *tb = addToolBar(name);
        tb->setObjectName(name);
        tb->setWindowTitle(name);
        toolbars.append(tb);

        if (i == 0) {
            fontCombo = new QFontComboBox(this);
            connect(fontCombo, &QFontComboBox::currentFontChanged, this, &MicroWriter::fontfamily);
            tb->addWidget(fontCombo);

            sizeSpin = new QSpinBox(this);
            sizeSpin->setRange(8, 72);
            sizeSpin->setValue(12);
            connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MicroWriter::fontSize);
            tb->addWidget(sizeSpin);

            tb->addSeparator();
        }

        for (const QString &key : actionKeys) {
            if (actionMap.contains(key)) {
                tb->addAction(actionMap[key]);
            }
        }
    }
    settings.endArray();

    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("windowState")) {
        restoreState(settings.value("windowState").toByteArray());
    }
}

void MicroWriter::closeEvent(QCloseEvent *event) {
    for (int i = 0; i < tabWidget->count(); ++i) {
        EditorView *ed = qobject_cast<EditorView*>(tabWidget->widget(i));
        if (ed && !maybeSave(ed)) {
            event->ignore();
            return;
        }
    }
    saveToolbarSettings();
    event->accept();
}

QMenu* MicroWriter::createPopupMenu() {
    QMenu *menu = new QMenu(this);

    for (QToolBar *tb : toolbars) {
        QAction *toggleAct = tb->toggleViewAction();
        toggleAct->setText(tb->windowTitle());
        menu->addAction(toggleAct);

        QAction *customAct = menu->addAction(tr("Customize %1...").arg(tb->windowTitle()));
        connect(customAct, &QAction::triggered, this, [=](){
            ToolbarCustomizeDialog dlg(tb, allMenuActions, this);
            if (dlg.exec() == QDialog::Accepted || true) {
                saveToolbarSettings();
            }
        });

        menu->addSeparator();
    }

    QAction *newTbAct = menu->addAction(tr("Create New Toolbar..."));
    connect(newTbAct, &QAction::triggered, this, &MicroWriter::createNewToolbar);

    return menu;
}

void MicroWriter::createNewToolbar() {
    bool ok;
    QString defaultName = QString("Toolbar %1").arg(toolbars.size() + 1);
    QString name = QInputDialog::getText(this, tr("Create New Toolbar"),
                                         tr("Toolbar Name:"), QLineEdit::Normal,
                                         defaultName, &ok);
    if (ok && !name.trimmed().isEmpty()) {
        QToolBar *tb = addToolBar(name.trimmed());
        tb->setObjectName(name.trimmed());
        tb->setWindowTitle(name.trimmed());
        toolbars.append(tb);

        ToolbarCustomizeDialog dlg(tb, allMenuActions, this);
        dlg.exec();
        saveToolbarSettings();
    }
}

void MicroWriter::newFile(const QString &title) {
    EditorView *editor = new EditorView(this);
    connect(editor, &QTextEdit::textChanged, this, &MicroWriter::updateStats);
    connect(editor, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateStats);
    connect(editor, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateFormatToolbarState);

    connect(editor->document(), &QTextDocument::modificationChanged, this, [=](bool changed){
        int idx = tabWidget->indexOf(editor);
        if (idx != -1) {
            QFileInfo fi(editor->filePath);
            QString name = editor->filePath.isEmpty() ? "Untitled" : fi.fileName();
            if (changed) name += " *";
            tabWidget->setTabText(idx, name);
        }
    });

    int index = tabWidget->addTab(editor, title);
    tabWidget->setCurrentIndex(index);
    ruler->setEditor(editor);
    applyLayoutModeToEditor(editor, currentLayoutMode);
    updateStats();
    updateFormatToolbarState();
}

void MicroWriter::openFile() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open Document",
        "",
        "Documents (*.html *.htm *.md *.txt);;All Files (*.*)",
        nullptr,
        QFileDialog::DontUseNativeDialog
    );
    if (!fileName.isEmpty()) {
        openFilePath(fileName);
    }
}

void MicroWriter::saveFile() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    if (editor->filePath.isEmpty()) {
        saveAsFile();
    } else {
        writeFile(editor->filePath);
    }
}

void MicroWriter::saveAsFile() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QString defaultPath = editor->filePath.isEmpty() ? "untitled.html" : editor->filePath;

    QFileDialog dialog(this, "Save Document As", defaultPath);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilters({"HTML Files (*.html)", "Markdown (*.md)", "Text Files (*.txt)", "All Files (*.*)"});

    if (dialog.exec() == QDialog::Accepted) {
        QString fileName = dialog.selectedFiles().first();
        QString selectedFilter = dialog.selectedNameFilter();

        QFileInfo fileInfo(fileName);
        if (fileInfo.suffix().isEmpty()) {
            if (selectedFilter.contains("*.md")) {
                fileName += ".md";
            } else if (selectedFilter.contains("*.txt")) {
                fileName += ".txt";
            } else {
                fileName += ".html";
            }
        }

        editor->filePath = fileName;
        writeFile(editor->filePath);

        QFileInfo updatedInfo(fileName);
        tabWidget->setTabText(tabWidget->currentIndex(), updatedInfo.fileName());
        updateWindowTitleAndStats();
    }
}

void MicroWriter::closeTabRequested(int index) {
    EditorView *editor = qobject_cast<EditorView*>(tabWidget->widget(index));
    if (editor && maybeSave(editor)) {
        tabWidget->removeTab(index);
        delete editor;
        if (tabWidget->count() == 0) {
            newFile();
        }
    }
}

bool MicroWriter::maybeSave(EditorView *editor) {
    if (!editor || !editor->document()->isModified())
        return true;

    QFileInfo fileInfo(editor->filePath);
    QString name = editor->filePath.isEmpty() ? "Untitled" : fileInfo.fileName();

    QMessageBox::StandardButton ret = QMessageBox::warning(
        this, "MicroWriter",
        tr("The document '%1' has been modified.\nDo you want to save your changes?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );

    if (ret == QMessageBox::Save) {
        tabWidget->setCurrentWidget(editor);
        saveFile();
        return !editor->document()->isModified();
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

EditorView* MicroWriter::currentEditor() {
    return qobject_cast<EditorView*>(tabWidget->currentWidget());
}

void MicroWriter::printDocument() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() == QDialog::Accepted) {
        editor->print(&printer);
    }
}

void MicroWriter::printPreview() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, this);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this, [editor](QPrinter *prn) {
        editor->print(prn);
    });
    preview.exec();
}

void MicroWriter::exportPdf() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QString defaultPath = "untitled.pdf";
    if (!editor->filePath.isEmpty()) {
        QFileInfo fi(editor->filePath);
        defaultPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".pdf";
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
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
            QMessageBox::critical(this, "Export PDF Error", "Could not initialize PDF painter engine.");
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
        QMessageBox::information(this, "Export PDF", "PDF exported successfully:\n" + fileName);
    } else {
        QMessageBox::critical(this, "Export PDF Error", "Could not generate PDF file.");
    }
}

void MicroWriter::exportOdt() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QString defaultPath = "untitled.odt";
    if (!editor->filePath.isEmpty()) {
        QFileInfo fi(editor->filePath);
        defaultPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".odt";
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
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
        QMessageBox::information(this, "Export ODT", "ODT file exported successfully:\n" + fileName);
    } else {
        QMessageBox::critical(this, "Export ODT Error", "Failed to write ODT document format.");
    }
}

void MicroWriter::insertImage() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Insert Image",
        "",
        "Images (*.png *.jpg *.jpeg *.bmp *.gif)",
        nullptr,
        QFileDialog::DontUseNativeDialog
    );
    if (!fileName.isEmpty()) {
        QImage image(fileName);
        if (!image.isNull()) {
            editor->textCursor().insertImage(image);
        }
    }
}

void MicroWriter::insertBulletList() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    QTextListFormat listFormat;
    listFormat.setStyle(QTextListFormat::ListDisc);
    cursor.createList(listFormat);
}

void MicroWriter::insertNumberedList() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    QTextListFormat listFormat;
    listFormat.setStyle(QTextListFormat::ListDecimal);
    cursor.createList(listFormat);
}

void MicroWriter::insertHorizontalLine() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    editor->textCursor().insertHtml("<hr>");
}

void MicroWriter::insertTable() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    TableDialog dlg(this);
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

void MicroWriter::insertDateTime() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    DateTimeDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString formatted = QDateTime::currentDateTime().toString(dlg.selectedFormatString());
        editor->textCursor().insertText(formatted);
    }
}

void MicroWriter::insertLink() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QTextCursor cursor = editor->textCursor();
    QString selectedText = cursor.hasSelection() ? cursor.selectedText() : "";

    LinkDialog dlg(selectedText, this);
    if (dlg.exec() == QDialog::Accepted) {
        QString url = dlg.url();
        QString text = dlg.linkText();
        if (!url.isEmpty()) {
            if (text.isEmpty()) text = url;
            QString html = QString("<a href=\"%1\">%2</a>").arg(url, text);
            cursor.insertHtml(html);
        }
    }
}

void MicroWriter::insertSymbol() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    SymbolDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString sym = dlg.getSelectedSymbol();
        if (!sym.isEmpty()) {
            editor->textCursor().insertText(sym);
        }
    }
}

void MicroWriter::configureHeaderFooter() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    HeaderFooterDialog dlg(editor->headerText(), editor->footerText(), this);
    if (dlg.exec() == QDialog::Accepted) {
        editor->setHeaderText(dlg.getHeader());
        editor->setFooterText(dlg.getFooter());
        editor->document()->setModified(true);
        editor->viewport()->update();
    }
}

void MicroWriter::textShading() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QColor col = QColorDialog::getColor(Qt::yellow, this, "Choose Text Shading Color");
    if (col.isValid()) {
        QTextCharFormat fmt;
        fmt.setBackground(col);
        mergeFormatOnWordOrSelection(fmt);
    }
}

void MicroWriter::textStrikethrough() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontStrikeOut(!editor->currentCharFormat().fontStrikeOut());
    mergeFormatOnWordOrSelection(fmt);
}

void MicroWriter::textSuperscript() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCharFormat fmt;
    int align = editor->currentCharFormat().verticalAlignment();
    if (align == QTextCharFormat::AlignSuperScript)
        fmt.setVerticalAlignment(QTextCharFormat::AlignNormal);
    else
        fmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    mergeFormatOnWordOrSelection(fmt);
}

void MicroWriter::textSubscript() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCharFormat fmt;
    int align = editor->currentCharFormat().verticalAlignment();
    if (align == QTextCharFormat::AlignSubScript)
        fmt.setVerticalAlignment(QTextCharFormat::AlignNormal);
    else
        fmt.setVerticalAlignment(QTextCharFormat::AlignSubScript);
    mergeFormatOnWordOrSelection(fmt);
}

void MicroWriter::insertBlockquote() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    cursor.insertHtml("<blockquote style=\"border-left: 3px solid #007acc; margin: 10px 0; padding-left: 10px; color: #555;\"><p>Type blockquote text here...</p></blockquote>");
}

void MicroWriter::toggleDoubleSpacing(bool doubleSpace) {
    EditorView *editor = currentEditor();
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

void MicroWriter::modifySelectedLines(std::function<QStringList(QStringList)> transformer) {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();

    QString text;
    bool hasSelection = cursor.hasSelection();
    if (hasSelection) {
        text = cursor.selectedText();
    } else {
        cursor.select(QTextCursor::Document);
        text = cursor.selectedText();
    }

    text.replace(QChar(0x2029), '\n');
    QStringList lines = text.split('\n');
    QStringList transformed = transformer(lines);
    QString newText = transformed.join('\n');

    cursor.beginEditBlock();
    if (hasSelection) {
        cursor.insertText(newText);
    } else {
        cursor.select(QTextCursor::Document);
        cursor.insertText(newText);
    }
    cursor.endEditBlock();
}

void MicroWriter::formatIndent() {
    modifySelectedLines([](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) out.append("    " + l);
        return out;
    });
}

void MicroWriter::formatUnindent() {
    modifySelectedLines([](QStringList lines) {
        QStringList out;
        for (QString l : lines) {
            if (l.startsWith("    ")) {
                l.remove(0, 4);
            } else if (l.startsWith('\t')) {
                l.remove(0, 1);
            }
            out.append(l);
        }
        return out;
    });
}

void MicroWriter::formatStripBlankLines() {
    modifySelectedLines([](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) {
            if (!l.trimmed().isEmpty()) {
                out.append(l);
            }
        }
        return out;
    });
}

void MicroWriter::toolsReverseLines() {
    modifySelectedLines([](QStringList lines) {
        std::reverse(lines.begin(), lines.end());
        return lines;
    });
}

void MicroWriter::toolsSortLines() {
    modifySelectedLines([](QStringList lines) {
        std::sort(lines.begin(), lines.end(), [](const QString &a, const QString &b) {
            return QString::compare(a, b, Qt::CaseInsensitive) < 0;
        });
        return lines;
    });
}

void MicroWriter::toolsUppercase() {
    modifySelectedLines([](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) out.append(l.toUpper());
        return out;
    });
}

void MicroWriter::toolsLowercase() {
    modifySelectedLines([](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) out.append(l.toLower());
        return out;
    });
}

void MicroWriter::toolsTitleCase() {
    modifySelectedLines([](QStringList lines) {
        QStringList out;
        for (const QString &l : lines) {
            QString title;
            bool capitalizeNext = true;
            for (int i = 0; i < l.length(); ++i) {
                QChar c = l[i];
                if (c.isSpace()) {
                    capitalizeNext = true;
                    title.append(c);
                } else {
                    if (capitalizeNext) {
                        title.append(c.toTitleCase());
                        capitalizeNext = false;
                    } else {
                        title.append(c.toLower());
                    }
                }
            }
            out.append(title);
        }
        return out;
    });
}

void MicroWriter::toolsAddPrefix() {
    bool ok;
    QString prefix = QInputDialog::getText(this, "Add Prefix", "Enter prefix string:", QLineEdit::Normal, "", &ok);
    if (ok) {
        modifySelectedLines([prefix](QStringList lines) {
            QStringList out;
            for (const QString &l : lines) out.append(prefix + l);
            return out;
        });
    }
}

void MicroWriter::toolsAddSuffix() {
    bool ok;
    QString suffix = QInputDialog::getText(this, "Add Suffix", "Enter suffix string:", QLineEdit::Normal, "", &ok);
    if (ok) {
        modifySelectedLines([suffix](QStringList lines) {
            QStringList out;
            for (const QString &l : lines) out.append(l + suffix);
            return out;
        });
    }
}

void MicroWriter::toolsTabsToSpaces() {
    modifySelectedLines([](QStringList lines) {
        QStringList out;
        for (QString l : lines) {
            out.append(l.replace('\t', "    "));
        }
        return out;
    });
}

void MicroWriter::toolsSpacesToTabs() {
    modifySelectedLines([](QStringList lines) {
        QStringList out;
        for (QString l : lines) {
            while (l.startsWith("    ")) {
                l.replace(0, 4, "\t");
            }
            out.append(l);
        }
        return out;
    });
}

void MicroWriter::setLayoutMode(LayoutMode mode) {
    currentLayoutMode = mode;
    for (int i = 0; i < tabWidget->count(); ++i) {
        EditorView *ed = qobject_cast<EditorView*>(tabWidget->widget(i));
        applyLayoutModeToEditor(ed, mode);
    }
}

void MicroWriter::applyLayoutModeToEditor(EditorView *editor, LayoutMode mode) {
    if (!editor) return;
    QTextDocument *doc = editor->document();

    QString bg = darkModeActive ? "#1e1e1e" : "#ffffff";
    QString textColor = darkModeActive ? "#dcdcdc" : "#000000";

    switch (mode) {
    case NormalLayout:
        editor->isPrintLayout = false;
        ruler->setVisible(true);
        editor->setStyleSheet(QString("QTextEdit { background-color: %1; color: %2; padding: 15px; }").arg(bg, textColor));
        doc->setPageSize(QSizeF());
        break;

    case WebLayout:
        editor->isPrintLayout = false;
        ruler->setVisible(false);
        editor->setStyleSheet(QString("QTextEdit { background-color: %1; color: %2; padding: 30px; }").arg(bg, textColor));
        doc->setPageSize(QSizeF());
        break;

    case PrintLayout:
        editor->isPrintLayout = true;
        ruler->setVisible(true);
        {
            QString deskBg = darkModeActive ? "#121212" : "#707070";
            QString paperBg = darkModeActive ? "#2b2b2b" : "#ffffff";
            editor->setStyleSheet(QString(
                "EditorView { background-color: %1; padding: 30px; }"
                "EditorView QTextEdit { background-color: %2; color: %3; border: 1px solid #404040; }"
            ).arg(deskBg, paperBg, textColor));
            doc->setPageSize(QSizeF(816, 1056));
        }
        break;
    }
    editor->viewport()->update();
}

void MicroWriter::toggleDarkMode(bool dark) {
    darkModeActive = dark;

    QSettings settings("MicroWriterOrg", "MicroWriterApp");
    settings.setValue("darkMode", dark);

    if (dark) {
        qApp->setStyleSheet(
            "QMainWindow { background-color: #2b2b2b; color: #ffffff; }"
            "QMenuBar { background-color: #3c3c3c; color: #ffffff; }"
            "QMenuBar::item:selected { background-color: #505050; }"
            "QMenu { background-color: #3c3c3c; color: #ffffff; }"
            "QMenu::item:selected { background-color: #505050; }"
            "QToolBar { background-color: #3c3c3c; border: none; }"
            "QStatusBar { background-color: #3c3c3c; color: #ffffff; }"
            "QTabWidget::pane { border: 1px solid #444444; }"
            "QTabBar::tab { background: #3c3c3c; color: #cccccc; padding: 6px 12px; border: 1px solid #444444; }"
            "QTabBar::tab:selected { background: #2b2b2b; color: #ffffff; border-bottom-color: #2b2b2b; }"
            "QPushButton { background-color: #444444; color: #ffffff; border: 1px solid #555555; padding: 4px; }"
            "QPushButton:hover { background-color: #555555; }"
            "QLineEdit, QComboBox { background-color: #1e1e1e; color: #ffffff; border: 1px solid #555555; }"
            "QSpinBox, QFontComboBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #555555; }"
        );
    } else {
        qApp->setStyleSheet("");
    }
    setLayoutMode(currentLayoutMode);
}

void MicroWriter::textBold() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontWeight(editor->currentCharFormat().fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
    mergeFormatOnWordOrSelection(fmt);
}

void MicroWriter::textItalic() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontItalic(!editor->currentCharFormat().fontItalic());
    mergeFormatOnWordOrSelection(fmt);
}

void MicroWriter::textUnderline() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QTextCharFormat fmt;
    fmt.setFontUnderline(!editor->currentCharFormat().fontUnderline());
    mergeFormatOnWordOrSelection(fmt);
}

void MicroWriter::textTextColor() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QColor col = QColorDialog::getColor(editor->textColor(), this, "Choose Text Color");
    if (col.isValid()) {
        QTextCharFormat fmt;
        fmt.setForeground(col);
        mergeFormatOnWordOrSelection(fmt);
    }
}

void MicroWriter::fontfamily(const QFont &font) {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QTextCharFormat fmt;
    fmt.setFontFamilies(QStringList{font.family()});

    // If the document is essentially empty, lock it as the default font too
    if (editor->toPlainText().trimmed().isEmpty()) {
        QFont docFont = editor->document()->defaultFont();
        docFont.setFamily(font.family());
        editor->document()->setDefaultFont(docFont);
    }

    mergeFormatOnWordOrSelection(fmt);
}

void MicroWriter::fontSize(int size) {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QTextCharFormat fmt;
    fmt.setFontPointSize(size);

    if (editor->toPlainText().trimmed().isEmpty()) {
        QFont docFont = editor->document()->defaultFont();
        docFont.setPointSize(size);
        editor->document()->setDefaultFont(docFont);
    }

    mergeFormatOnWordOrSelection(fmt);
}

void MicroWriter::alignLeft() { if (currentEditor()) currentEditor()->setAlignment(Qt::AlignLeft); }
void MicroWriter::alignCenter() { if (currentEditor()) currentEditor()->setAlignment(Qt::AlignCenter); }
void MicroWriter::alignRight() { if (currentEditor()) currentEditor()->setAlignment(Qt::AlignRight); }
void MicroWriter::alignJustify() { if (currentEditor()) currentEditor()->setAlignment(Qt::AlignJustify); }

void MicroWriter::showFindDialog() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    FindReplaceDialog *dlg = new FindReplaceDialog(editor, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MicroWriter::updateStats() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    QString text = editor->toPlainText();
    int charCount = text.length();
    int wordCount = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
    if (text.trimmed().isEmpty()) {
        wordCount = 0;
    }

    QTextCursor cursor = editor->textCursor();
    int lineNum = cursor.blockNumber() + 1;
    int colNum = cursor.columnNumber() + 1;

    QString eolStr = "LF";
    if (editor->eolFormat == EolCrlf) eolStr = "CRLF";
    else if (editor->eolFormat == EolCr) eolStr = "CR";

    statusLabel->setText(QString("Line: %1, Col: %2 | Words: %3 | Chars: %4 | Enc: %5 | EOL: %6")
        .arg(lineNum).arg(colNum).arg(wordCount).arg(charCount).arg(editor->encodingName).arg(eolStr));
}

void MicroWriter::updateFormatToolbarState() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QTextCharFormat fmt = editor->currentCharFormat();

    if (fontCombo) {
        fontCombo->blockSignals(true);
        QStringList families = fmt.fontFamilies().toStringList();
        if (!families.isEmpty()) {
            fontCombo->setCurrentFont(QFont(families.first()));
        } else {
            fontCombo->setCurrentFont(fmt.font());
        }
        fontCombo->blockSignals(false);
    }

    if (sizeSpin) {
        sizeSpin->blockSignals(true);
        int pt = qRound(fmt.fontPointSize() > 0 ? fmt.fontPointSize() : fmt.font().pointSize());
        if (pt > 0) {
            sizeSpin->setValue(pt);
        }
        sizeSpin->blockSignals(false);
    }
}

void MicroWriter::updateWindowTitleAndStats() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    if (editor->filePath.isEmpty()) {
        setWindowTitle("Qt6 MicroWriter - New File");
    } else {
        setWindowTitle("Qt6 MicroWriter - " + editor->filePath);
    }
    updateStats();
}

void MicroWriter::writeFile(const QString &fileName) {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString content;
        if (fileName.endsWith(".md")) {
            content = editor->toMarkdown();
        } else if (fileName.endsWith(".html") || fileName.endsWith(".htm")) {
            content = editor->toHtml();
            content.replace(
                QRegularExpression("<!DOCTYPE[^>]*>", QRegularExpression::CaseInsensitiveOption),
                "<!DOCTYPE html>"
            );
        } else {
            content = editor->toPlainText();
        }

        content.replace(QRegularExpression("\\r\\n|\\r"), "\n");
        if (editor->eolFormat == EolCrlf) {
            content.replace('\n', "\r\n");
        } else if (editor->eolFormat == EolCr) {
            content.replace('\n', "\r");
        } else {
            content.replace('\n', "\n");
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::encodingForName(editor->encodingName.toUtf8()).value_or(QStringConverter::Utf8));
        out << content;

        editor->document()->setModified(false);
    }
}

void MicroWriter::mergeFormatOnWordOrSelection(const QTextCharFormat &format) {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QTextCursor cursor = editor->textCursor();

    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(format);
    } else {
        // 1. Set the active typing format
        editor->mergeCurrentCharFormat(format);

        // 2. Lock it onto the current block so hitting Enter or moving within the block persists it
        QTextBlockFormat bFmt = cursor.blockFormat();
        cursor.setBlockCharFormat(format);
    }

    editor->mergeCurrentCharFormat(format);
    updateFormatToolbarState();
}

void MicroWriter::registerAction(QAction *act) {
    if (act && !act->text().isEmpty()) {
        allMenuActions.append(act);
        QString key = act->text();
        key.remove('&');
        key = key.trimmed();
        act->setObjectName(key);
        actionMap[key] = act;
    }
}

void MicroWriter::createMenus() {
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *newAction = new QAction(tr("&New"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [=](){ newFile(); });
    fileMenu->addAction(newAction);
    registerAction(newAction);

    QAction *openAction = new QAction(tr("&Open..."), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MicroWriter::openFile);
    fileMenu->addAction(openAction);
    registerAction(openAction);

    QAction *saveAction = new QAction(tr("&Save"), this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MicroWriter::saveFile);
    fileMenu->addAction(saveAction);
    registerAction(saveAction);

    QAction *saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MicroWriter::saveAsFile);
    fileMenu->addAction(saveAsAct);
    registerAction(saveAsAct);

    fileMenu->addSeparator();

    QMenu *encodingMenu = fileMenu->addMenu(tr("Text &Encoding"));
    QActionGroup *encGroup = new QActionGroup(this);
    QStringList encodings = {"UTF-8", "UTF-16", "ISO-8859-1", "Windows-1252"};
    for (const QString &enc : encodings) {
        QAction *encAct = new QAction(enc, this);
        encAct->setCheckable(true);
        if (enc == "UTF-8") encAct->setChecked(true);
        encGroup->addAction(encAct);
        encodingMenu->addAction(encAct);
        connect(encAct, &QAction::triggered, this, [=](){
            EditorView *ed = currentEditor();
            if (ed) {
                ed->encodingName = enc;
                updateStats();
            }
        });
    }

    QMenu *eolMenu = fileMenu->addMenu(tr("&Line Endings (EOL)"));
    QActionGroup *eolGroup = new QActionGroup(this);

    QAction *lfAct = new QAction(tr("Unix (LF / \\n)"), this);
    lfAct->setCheckable(true);
    lfAct->setChecked(true);
    eolGroup->addAction(lfAct);
    eolMenu->addAction(lfAct);
    connect(lfAct, &QAction::triggered, this, [=](){
        EditorView *ed = currentEditor();
        if (ed) { ed->eolFormat = EolLf; updateStats(); }
    });

    QAction *crlfAct = new QAction(tr("Windows (CRLF / \\r\\n)"), this);
    crlfAct->setCheckable(true);
    eolGroup->addAction(crlfAct);
    eolMenu->addAction(crlfAct);
    connect(crlfAct, &QAction::triggered, this, [=](){
        EditorView *ed = currentEditor();
        if (ed) { ed->eolFormat = EolCrlf; updateStats(); }
    });

    QAction *crAct = new QAction(tr("Mac Classic (CR / \\r)"), this);
    crAct->setCheckable(true);
    eolGroup->addAction(crAct);
    eolMenu->addAction(crAct);
    connect(crAct, &QAction::triggered, this, [=](){
        EditorView *ed = currentEditor();
        if (ed) { ed->eolFormat = EolCr; updateStats(); }
    });

    fileMenu->addSeparator();

    QAction *printPreviewAct = new QAction(tr("Print Pre&view..."), this);
    connect(printPreviewAct, &QAction::triggered, this, &MicroWriter::printPreview);
    fileMenu->addAction(printPreviewAct);
    registerAction(printPreviewAct);

    QAction *printAct = new QAction(tr("&Print..."), this);
    printAct->setShortcut(QKeySequence::Print);
    connect(printAct, &QAction::triggered, this, &MicroWriter::printDocument);
    fileMenu->addAction(printAct);
    registerAction(printAct);

    QAction *pdfAct = new QAction(tr("Export as &PDF..."), this);
    connect(pdfAct, &QAction::triggered, this, &MicroWriter::exportPdf);
    fileMenu->addAction(pdfAct);
    registerAction(pdfAct);

    QAction *odtAct = new QAction(tr("Export as &ODT..."), this);
    connect(odtAct, &QAction::triggered, this, &MicroWriter::exportOdt);
    fileMenu->addAction(odtAct);
    registerAction(odtAct);

    fileMenu->addSeparator();

    QAction *exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAct);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction *undoAct = new QAction(tr("&Undo"), this);
    undoAct->setShortcut(QKeySequence::Undo);
    connect(undoAct, &QAction::triggered, this, [=](){ if (currentEditor()) currentEditor()->undo(); });
    editMenu->addAction(undoAct);
    registerAction(undoAct);

    QAction *redoAct = new QAction(tr("&Redo"), this);
    redoAct->setShortcut(QKeySequence::Redo);
    connect(redoAct, &QAction::triggered, this, [=](){ if (currentEditor()) currentEditor()->redo(); });
    editMenu->addAction(redoAct);
    registerAction(redoAct);

    editMenu->addSeparator();

    QAction *findAct = new QAction(tr("&Find & Replace..."), this);
    findAct->setShortcut(QKeySequence::Find);
    connect(findAct, &QAction::triggered, this, &MicroWriter::showFindDialog);
    editMenu->addAction(findAct);
    registerAction(findAct);

    QMenu *formatMenu = menuBar()->addMenu(tr("&Format"));

    QAction *boldAct = new QAction(tr("&Bold"), this);
    boldAct->setShortcut(QKeySequence::Bold);
    connect(boldAct, &QAction::triggered, this, &MicroWriter::textBold);
    formatMenu->addAction(boldAct);
    registerAction(boldAct);

    QAction *italicAct = new QAction(tr("&Italic"), this);
    italicAct->setShortcut(QKeySequence::Italic);
    connect(italicAct, &QAction::triggered, this, &MicroWriter::textItalic);
    formatMenu->addAction(italicAct);
    registerAction(italicAct);

    QAction *underlineAct = new QAction(tr("&Underline"), this);
    underlineAct->setShortcut(QKeySequence::Underline);
    connect(underlineAct, &QAction::triggered, this, &MicroWriter::textUnderline);
    formatMenu->addAction(underlineAct);
    registerAction(underlineAct);

    QAction *strikeAct = new QAction(tr("Strikethrough"), this);
    connect(strikeAct, &QAction::triggered, this, &MicroWriter::textStrikethrough);
    formatMenu->addAction(strikeAct);
    registerAction(strikeAct);

    QAction *superAct = new QAction(tr("Superscript"), this);
    connect(superAct, &QAction::triggered, this, &MicroWriter::textSuperscript);
    formatMenu->addAction(superAct);
    registerAction(superAct);

    QAction *subAct = new QAction(tr("Subscript"), this);
    connect(subAct, &QAction::triggered, this, &MicroWriter::textSubscript);
    formatMenu->addAction(subAct);
    registerAction(subAct);

    formatMenu->addSeparator();

    QAction *colorAct = new QAction(tr("Text &Color..."), this);
    connect(colorAct, &QAction::triggered, this, &MicroWriter::textTextColor);
    formatMenu->addAction(colorAct);
    registerAction(colorAct);

    QAction *shadingAct = new QAction(tr("Text &Shading..."), this);
    connect(shadingAct, &QAction::triggered, this, &MicroWriter::textShading);
    formatMenu->addAction(shadingAct);
    registerAction(shadingAct);

    formatMenu->addSeparator();

    QAction *leftAct = new QAction(tr("Align &Left"), this);
    connect(leftAct, &QAction::triggered, this, &MicroWriter::alignLeft);
    formatMenu->addAction(leftAct);
    registerAction(leftAct);

    QAction *centerAct = new QAction(tr("Align &Center"), this);
    connect(centerAct, &QAction::triggered, this, &MicroWriter::alignCenter);
    formatMenu->addAction(centerAct);
    registerAction(centerAct);

    QAction *rightAct = new QAction(tr("Align &Right"), this);
    connect(rightAct, &QAction::triggered, this, &MicroWriter::alignRight);
    formatMenu->addAction(rightAct);
    registerAction(rightAct);

    QAction *justifyAct = new QAction(tr("&Justify"), this);
    connect(justifyAct, &QAction::triggered, this, &MicroWriter::alignJustify);
    formatMenu->addAction(justifyAct);
    registerAction(justifyAct);

    formatMenu->addSeparator();

    QAction *doubleSpaceAct = new QAction(tr("Double Spacing"), this);
    doubleSpaceAct->setCheckable(true);
    connect(doubleSpaceAct, &QAction::triggered, this, &MicroWriter::toggleDoubleSpacing);
    formatMenu->addAction(doubleSpaceAct);
    registerAction(doubleSpaceAct);

    QAction *indentAct = new QAction(tr("Indent"), this);
    connect(indentAct, &QAction::triggered, this, &MicroWriter::formatIndent);
    formatMenu->addAction(indentAct);
    registerAction(indentAct);

    QAction *unindentAct = new QAction(tr("Unindent"), this);
    connect(unindentAct, &QAction::triggered, this, &MicroWriter::formatUnindent);
    formatMenu->addAction(unindentAct);
    registerAction(unindentAct);

    QAction *stripBlankAct = new QAction(tr("Strip Blank Lines"), this);
    connect(stripBlankAct, &QAction::triggered, this, &MicroWriter::formatStripBlankLines);
    formatMenu->addAction(stripBlankAct);
    registerAction(stripBlankAct);

    formatMenu->addSeparator();

    QAction *bulletAct = new QAction(tr("Bullet &List"), this);
    connect(bulletAct, &QAction::triggered, this, &MicroWriter::insertBulletList);
    formatMenu->addAction(bulletAct);
    registerAction(bulletAct);

    QAction *numberedAct = new QAction(tr("&Numbered List"), this);
    connect(numberedAct, &QAction::triggered, this, &MicroWriter::insertNumberedList);
    formatMenu->addAction(numberedAct);
    registerAction(numberedAct);

    QAction *blockquoteAct = new QAction(tr("Blockquote"), this);
    connect(blockquoteAct, &QAction::triggered, this, &MicroWriter::insertBlockquote);
    formatMenu->addAction(blockquoteAct);
    registerAction(blockquoteAct);

    QAction *hrAct = new QAction(tr("Horizontal &Line"), this);
    connect(hrAct, &QAction::triggered, this, &MicroWriter::insertHorizontalLine);
    formatMenu->addAction(hrAct);
    registerAction(hrAct);

    QAction *tableAct = new QAction(tr("Insert &Table..."), this);
    connect(tableAct, &QAction::triggered, this, &MicroWriter::insertTable);
    formatMenu->addAction(tableAct);
    registerAction(tableAct);

    QAction *dateTimeAct = new QAction(tr("Insert &Date & Time..."), this);
    connect(dateTimeAct, &QAction::triggered, this, &MicroWriter::insertDateTime);
    formatMenu->addAction(dateTimeAct);
    registerAction(dateTimeAct);

    QAction *linkAct = new QAction(tr("Insert &Link..."), this);
    connect(linkAct, &QAction::triggered, this, &MicroWriter::insertLink);
    formatMenu->addAction(linkAct);
    registerAction(linkAct);

    QAction *symbolAct = new QAction(tr("Insert &Symbol..."), this);
    connect(symbolAct, &QAction::triggered, this, &MicroWriter::insertSymbol);
    formatMenu->addAction(symbolAct);
    registerAction(symbolAct);

    formatMenu->addSeparator();

    QAction *imgAct = new QAction(tr("Insert &Image..."), this);
    connect(imgAct, &QAction::triggered, this, &MicroWriter::insertImage);
    formatMenu->addAction(imgAct);
    registerAction(imgAct);

    QAction *headerFooterAct = new QAction(tr("Header & Footer..."), this);
    connect(headerFooterAct, &QAction::triggered, this, &MicroWriter::configureHeaderFooter);
    formatMenu->addAction(headerFooterAct);
    registerAction(headerFooterAct);

    // Tools Menu
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));

    QAction *sortLinesAct = new QAction(tr("Sort Lines (Ignore Case)"), this);
    connect(sortLinesAct, &QAction::triggered, this, &MicroWriter::toolsSortLines);
    toolsMenu->addAction(sortLinesAct);
    registerAction(sortLinesAct);

    QAction *reverseLinesAct = new QAction(tr("Reverse Lines"), this);
    connect(reverseLinesAct, &QAction::triggered, this, &MicroWriter::toolsReverseLines);
    toolsMenu->addAction(reverseLinesAct);
    registerAction(reverseLinesAct);

    toolsMenu->addSeparator();

    QMenu *caseMenu = toolsMenu->addMenu(tr("Fix Case"));
    QAction *upperAct = new QAction(tr("UPPERCASE"), this);
    connect(upperAct, &QAction::triggered, this, &MicroWriter::toolsUppercase);
    caseMenu->addAction(upperAct);
    registerAction(upperAct);

    QAction *lowerAct = new QAction(tr("lowercase"), this);
    connect(lowerAct, &QAction::triggered, this, &MicroWriter::toolsLowercase);
    caseMenu->addAction(lowerAct);
    registerAction(lowerAct);

    QAction *titleAct = new QAction(tr("Title Case"), this);
    connect(titleAct, &QAction::triggered, this, &MicroWriter::toolsTitleCase);
    caseMenu->addAction(titleAct);
    registerAction(titleAct);

    toolsMenu->addSeparator();

    QAction *prefixAct = new QAction(tr("Add Prefix..."), this);
    connect(prefixAct, &QAction::triggered, this, &MicroWriter::toolsAddPrefix);
    toolsMenu->addAction(prefixAct);
    registerAction(prefixAct);

    QAction *suffixAct = new QAction(tr("Add Suffix..."), this);
    connect(suffixAct, &QAction::triggered, this, &MicroWriter::toolsAddSuffix);
    toolsMenu->addAction(suffixAct);
    registerAction(suffixAct);

    toolsMenu->addSeparator();

    QAction *tabsToSpacesAct = new QAction(tr("Convert Tabs to Spaces"), this);
    connect(tabsToSpacesAct, &QAction::triggered, this, &MicroWriter::toolsTabsToSpaces);
    toolsMenu->addAction(tabsToSpacesAct);
    registerAction(tabsToSpacesAct);

    QAction *spacesToTabsAct = new QAction(tr("Convert Spaces to Tabs"), this);
    connect(spacesToTabsAct, &QAction::triggered, this, &MicroWriter::toolsSpacesToTabs);
    toolsMenu->addAction(spacesToTabsAct);
    registerAction(spacesToTabsAct);

    // View Menu
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    QAction *newTbAct = new QAction(tr("Create New Toolbar..."), this);
    connect(newTbAct, &QAction::triggered, this, &MicroWriter::createNewToolbar);
    viewMenu->addAction(newTbAct);

    viewMenu->addSeparator();

    QMenu *layoutMenu = viewMenu->addMenu(tr("&Layout Mode"));
    QActionGroup *layoutGroup = new QActionGroup(this);

    QAction *normalLayoutAct = new QAction(tr("&Normal Layout"), this);
    normalLayoutAct->setCheckable(true);
    normalLayoutAct->setChecked(true);
    layoutGroup->addAction(normalLayoutAct);
    layoutMenu->addAction(normalLayoutAct);
    connect(normalLayoutAct, &QAction::triggered, this, [=](){ setLayoutMode(NormalLayout); });

    QAction *webLayoutAct = new QAction(tr("&Web Layout"), this);
    webLayoutAct->setCheckable(true);
    layoutGroup->addAction(webLayoutAct);
    layoutMenu->addAction(webLayoutAct);
    connect(webLayoutAct, &QAction::triggered, this, [=](){ setLayoutMode(WebLayout); });

    QAction *printLayoutAct = new QAction(tr("&Print Layout"), this);
    printLayoutAct->setCheckable(true);
    layoutGroup->addAction(printLayoutAct);
    layoutMenu->addAction(printLayoutAct);
    connect(printLayoutAct, &QAction::triggered, this, [=](){ setLayoutMode(PrintLayout); });

    viewMenu->addSeparator();

    darkAct = new QAction(tr("Dark Mode"), this);
    darkAct->setCheckable(true);
    connect(darkAct, &QAction::triggered, this, &MicroWriter::toggleDarkMode);
    viewMenu->addAction(darkAct);

    // Help Menu
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *regexHelpAct = new QAction(tr("Regex Cheat Sheet..."), this);
    connect(regexHelpAct, &QAction::triggered, this, [=](){
        EditorView *ed = currentEditor();
        if (!ed) return;
        RegexHelpDialog dlg(ed, this);
        dlg.exec();
    });
    helpMenu->addAction(regexHelpAct);
}

void MicroWriter::createDefaultToolBar() {
    QToolBar *tb1 = addToolBar(tr("Toolbar 1"));
    tb1->setObjectName("Toolbar 1");
    tb1->setWindowTitle("Toolbar 1");
    toolbars.append(tb1);

    fontCombo = new QFontComboBox(this);
    connect(fontCombo, &QFontComboBox::currentFontChanged, this, &MicroWriter::fontfamily);
    tb1->addWidget(fontCombo);

    sizeSpin = new QSpinBox(this);
    sizeSpin->setRange(8, 72);
    sizeSpin->setValue(12);
    connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MicroWriter::fontSize);
    tb1->addWidget(sizeSpin);

    tb1->addSeparator();

    for (QAction *act : allMenuActions) {
        if (act->text().contains("Bold") || act->text().contains("Italic") || act->text().contains("Underline") ||
            act->text().contains("Left") || act->text().contains("Center") || act->text().contains("Right") || act->text().contains("Justify")) {
            tb1->addAction(act);
        }
    }
}

void MicroWriter::createStatusBar() {
    statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(statusLabel);
}
