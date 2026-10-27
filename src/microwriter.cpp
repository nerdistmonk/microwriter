#include "microwriter.h"
#include "editorview.h"
#include "codeeditorview.h"
#include "ruler.h"
#include "dialogs.h"
#include "documentformatter.h"
#include "actionmanager.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QTextStream>
#include <QFontComboBox>
#include <QSpinBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QStringConverter>
#include <QRegularExpression>
#include <QSignalBlocker>

MicroWriter::MicroWriter(QWidget *parent)
    : QMainWindow(parent),
      ruler(nullptr),
      tabWidget(nullptr),
      statusLabel(nullptr),
      fontCombo(nullptr),
      sizeSpin(nullptr),
      actionManager(nullptr),
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
        Q_UNUSED(index);
        updateWindowTitleAndStats();
        BaseEditor *base = currentBaseEditor();
        if (!base) return;

        bool isRich = base->supportsRichFormatting();
        if (fontCombo) fontCombo->setEnabled(isRich);
        if (sizeSpin) sizeSpin->setEnabled(isRich);

        if (isRich) {
            EditorView *ed = dynamic_cast<EditorView*>(base);
            ruler->setEditor(ed);
            applyLayoutModeToEditor(ed, currentLayoutMode);
            updateFormatToolbarState();
        } else {
            ruler->setVisible(false);
        }
    });

    actionManager = new ActionManager(this);
    actionManager->setupMenus(menuBar());

    loadToolbarSettings();
    createStatusBar();

    resize(1100, 750);
    newFile();

    QSettings settings("MicroWriterOrg", "MicroWriterApp");
    if (settings.value("darkMode", false).toBool()) {
        if (darkAct) darkAct->setChecked(true);
        toggleDarkMode(true);
    }
}

BaseEditor* MicroWriter::currentBaseEditor() const {
    QWidget *w = tabWidget->currentWidget();
    return dynamic_cast<BaseEditor*>(w);
}

EditorView* MicroWriter::currentEditor() const {
    QWidget *w = tabWidget->currentWidget();
    return dynamic_cast<EditorView*>(w);
}

void MicroWriter::fontfamily(const QFont &font) {
    DocumentFormatter::applyFontFamily(currentEditor(), font);
    updateFormatToolbarState();
}

void MicroWriter::fontSize(int size) {
    DocumentFormatter::applyFontSize(currentEditor(), size);
    updateFormatToolbarState();
}

void MicroWriter::attachEditorSignals(BaseEditor *editor) {
    QWidget *w = editor->asWidget();

    auto updateTabTitle = [=](bool changed) {
        int idx = tabWidget->indexOf(w);
        if (idx != -1) {
            QString name = editor->filePath().isEmpty() ? "Untitled" : QFileInfo(editor->filePath()).fileName();
            if (changed) {
                name += " *";
            }
            tabWidget->setTabText(idx, name);
        }
    };

    if (EditorView *ed = dynamic_cast<EditorView*>(editor)) {
        connect(ed, &QTextEdit::textChanged, this, &MicroWriter::updateStats);
        connect(ed, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateStats);
        connect(ed, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateFormatToolbarState);
        connect(ed->document(), &QTextDocument::modificationChanged, this, updateTabTitle);
    } else if (CodeEditorView *ce = dynamic_cast<CodeEditorView*>(editor)) {
        connect(ce, &QPlainTextEdit::textChanged, this, &MicroWriter::updateStats);
        connect(ce, &QPlainTextEdit::cursorPositionChanged, this, &MicroWriter::updateStats);
        connect(ce->document(), &QTextDocument::modificationChanged, this, updateTabTitle);
    }
}

void MicroWriter::openFilePath(const QString &fileName) {
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QByteArray rawData = file.readAll();
    EolFormat eol = EolLf;
    if (rawData.contains("\r\n")) eol = EolCrlf;
    else if (rawData.contains('\r') && !rawData.contains('\n')) eol = EolCr;

    QString encName = "UTF-8";
    QTextStream in(&rawData);
    in.setEncoding(QStringConverter::encodingForName(encName.toUtf8()).value_or(QStringConverter::Utf8));
    QString content = in.readAll();

    BaseEditor *newBase = nullptr;
    QWidget *newWidget = nullptr;

    if (currentLayoutMode == CodeLayout) {
        CodeEditorView *codeEd = new CodeEditorView(this);
        codeEd->setFilePath(fileName);
        codeEd->setEncodingName(encName);
        codeEd->setEolFormat(eol);
        codeEd->setContent(content, fileName);
        codeEd->setDarkMode(darkModeActive);

        newBase = codeEd;
        newWidget = codeEd;
        ruler->setVisible(false);
        if (fontCombo) fontCombo->setEnabled(false);
        if (sizeSpin) sizeSpin->setEnabled(false);
    } else {
        EditorView *editor = new EditorView(this);
        editor->setFilePath(fileName);
        editor->setEncodingName(encName);
        editor->setEolFormat(eol);
        editor->setContent(content, fileName);
        applyLayoutModeToEditor(editor, currentLayoutMode);

        newBase = editor;
        newWidget = editor;
        ruler->setEditor(editor);
        if (fontCombo) fontCombo->setEnabled(true);
        if (sizeSpin) sizeSpin->setEnabled(true);
    }

    attachEditorSignals(newBase);

    // If there is only one tab and it's an untouched blank file, replace it
    if (tabWidget->count() == 1) {
        BaseEditor *firstEd = dynamic_cast<BaseEditor*>(tabWidget->widget(0));
        if (firstEd && firstEd->filePath().isEmpty() && firstEd->getRawPlainText().trimmed().isEmpty()) {
            tabWidget->removeTab(0);
            delete firstEd->asWidget();
        }
    }

    int index = tabWidget->addTab(newWidget, QFileInfo(fileName).fileName());
    tabWidget->setCurrentIndex(index);
    updateWindowTitleAndStats();
    if (currentLayoutMode != CodeLayout) {
        updateFormatToolbarState();
    }

    // Ensure newly opened document begins in a clean, unmodified state
    newBase->setModified(false);
    tabWidget->setTabText(index, QFileInfo(fileName).fileName());
}

void MicroWriter::newFile(const QString &title) {
    BaseEditor *newBase = nullptr;
    QWidget *newWidget = nullptr;

    if (currentLayoutMode == CodeLayout) {
        CodeEditorView *codeEd = new CodeEditorView(this);
        codeEd->setDarkMode(darkModeActive);
        newBase = codeEd;
        newWidget = codeEd;

        ruler->setVisible(false);
        if (fontCombo) fontCombo->setEnabled(false);
        if (sizeSpin) sizeSpin->setEnabled(false);
    } else {
        EditorView *editor = new EditorView(this);
        applyLayoutModeToEditor(editor, currentLayoutMode);
        newBase = editor;
        newWidget = editor;

        ruler->setEditor(editor);
        if (fontCombo) fontCombo->setEnabled(true);
        if (sizeSpin) sizeSpin->setEnabled(true);
    }

    attachEditorSignals(newBase);

    int index = tabWidget->addTab(newWidget, title);
    tabWidget->setCurrentIndex(index);
    updateWindowTitleAndStats();
    if (currentLayoutMode != CodeLayout) {
        updateFormatToolbarState();
    }

    // Ensure newly created document begins in a clean, unmodified state
    newBase->setModified(false);
    tabWidget->setTabText(index, title);
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
    if (!fileName.isEmpty()) openFilePath(fileName);
}

void MicroWriter::saveFile() {
    BaseEditor *editor = currentBaseEditor();
    if (!editor) return;

    if (editor->filePath().isEmpty()) saveAsFile();
    else writeFile(editor->filePath());
}

void MicroWriter::saveAsFile() {
    BaseEditor *editor = currentBaseEditor();
    if (!editor) return;

    QString defaultPath = editor->filePath().isEmpty() ? "untitled.html" : editor->filePath();
    QFileDialog dialog(this, "Save Document As", defaultPath);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilters({"HTML Files (*.html)", "Markdown (*.md)", "Text Files (*.txt)", "All Files (*.*)"});

    if (dialog.exec() == QDialog::Accepted) {
        QString fileName = dialog.selectedFiles().first();
        QString selectedFilter = dialog.selectedNameFilter();

        QFileInfo fileInfo(fileName);
        if (fileInfo.suffix().isEmpty()) {
            if (selectedFilter.contains("*.md")) fileName += ".md";
            else if (selectedFilter.contains("*.txt")) fileName += ".txt";
            else fileName += ".html";
        }

        editor->setFilePath(fileName);
        writeFile(fileName);
        tabWidget->setTabText(tabWidget->currentIndex(), QFileInfo(fileName).fileName());
        updateWindowTitleAndStats();
    }
}

void MicroWriter::writeFile(const QString &fileName) {
    BaseEditor *editor = currentBaseEditor();
    if (!editor) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QString content = editor->getContentForSave();

    content.replace(QRegularExpression("\\r\\n|\\r"), "\n");
    if (editor->eolFormat() == EolCrlf) content.replace('\n', "\r\n");
    else if (editor->eolFormat() == EolCr) content.replace('\n', "\r");

    QTextStream out(&file);
    out.setEncoding(QStringConverter::encodingForName(editor->encodingName().toUtf8()).value_or(QStringConverter::Utf8));
    out << content;
    editor->setModified(false);
}

void MicroWriter::closeTabRequested(int index) {
    if (maybeSaveTab(index)) {
        QWidget *w = tabWidget->widget(index);
        tabWidget->removeTab(index);
        delete w;
        if (tabWidget->count() == 0) newFile();
    }
}

bool MicroWriter::maybeSaveTab(int index) {
    QWidget *w = tabWidget->widget(index);
    BaseEditor *editor = dynamic_cast<BaseEditor*>(w);
    if (!editor || !editor->isModified()) return true;

    if (editor->filePath().isEmpty() && editor->getRawPlainText().trimmed().isEmpty()) {
        return true;
    }

    QString name = editor->filePath().isEmpty() ? "Untitled" : QFileInfo(editor->filePath()).fileName();
    QMessageBox::StandardButton ret = QMessageBox::warning(this, "MicroWriter",
        tr("The document '%1' has been modified.\nDo you want to save your changes?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save) {
        tabWidget->setCurrentIndex(index);
        saveFile();
        return !editor->isModified();
    }
    return ret != QMessageBox::Cancel;
}

void MicroWriter::setLayoutMode(LayoutMode mode) {
    currentLayoutMode = mode;
    int curIdx = tabWidget->currentIndex();
    if (curIdx == -1) return;

    BaseEditor *curBase = currentBaseEditor();
    if (!curBase) return;

    if (mode == CodeLayout) {
        if (curBase->supportsRichFormatting()) {
            QString rawContent = curBase->getContentForSave();
            QString fPath = curBase->filePath();
            QString enc = curBase->encodingName();
            EolFormat eol = curBase->eolFormat();
            bool modified = curBase->isModified();
            QString tabTitle = tabWidget->tabText(curIdx);

            CodeEditorView *codeEd = new CodeEditorView(this);
            codeEd->setFilePath(fPath);
            codeEd->setEncodingName(enc);
            codeEd->setEolFormat(eol);
            codeEd->setContent(rawContent, fPath);
            codeEd->setModified(modified);
            codeEd->setDarkMode(darkModeActive);

            attachEditorSignals(codeEd);

            tabWidget->removeTab(curIdx);
            delete curBase->asWidget();

            tabWidget->insertTab(curIdx, codeEd, tabTitle);
            tabWidget->setCurrentIndex(curIdx);

            ruler->setVisible(false);
            if (fontCombo) fontCombo->setEnabled(false);
            if (sizeSpin) sizeSpin->setEnabled(false);
            updateWindowTitleAndStats();
        }
    } else {
        if (!curBase->supportsRichFormatting()) {
            QString rawContent = curBase->getRawPlainText();
            QString fPath = curBase->filePath();
            QString enc = curBase->encodingName();
            EolFormat eol = curBase->eolFormat();
            bool modified = curBase->isModified();
            QString tabTitle = tabWidget->tabText(curIdx);

            EditorView *editor = new EditorView(this);
            editor->setFilePath(fPath);
            editor->setEncodingName(enc);
            editor->setEolFormat(eol);

            attachEditorSignals(editor);

            tabWidget->removeTab(curIdx);
            delete curBase->asWidget();

            tabWidget->insertTab(curIdx, editor, tabTitle);
            tabWidget->setCurrentIndex(curIdx);

            ruler->setEditor(editor);
            applyLayoutModeToEditor(editor, mode);
            editor->setContent(rawContent, fPath);
            editor->setModified(modified);

            if (fontCombo) fontCombo->setEnabled(true);
            if (sizeSpin) sizeSpin->setEnabled(true);
            updateWindowTitleAndStats();
            updateFormatToolbarState();
        } else {
            if (EditorView *ed = dynamic_cast<EditorView*>(curBase)) {
                applyLayoutModeToEditor(ed, mode);
            }
        }
    }
}

void MicroWriter::applyLayoutModeToEditor(EditorView *editor, LayoutMode mode) {
    if (!editor) return;
    QTextDocument *doc = editor->document();
    bool wasModified = doc->isModified();

    // Prevent frame formatting and layout invalidation from dirtying the document modification flag
    bool docSignalsBlocked = doc->blockSignals(true);

    QString bg = darkModeActive ? "#1e1e1e" : "#ffffff";
    QString textCol = darkModeActive ? "#dcdcdc" : "#000000";

    QTextFrameFormat frameFmt = doc->rootFrame()->frameFormat();

    switch (mode) {
    case NormalLayout:
        editor->isPrintLayout = false;
        ruler->setVisible(true);
        editor->setStyleSheet(QString("EditorView { background-color: %1; color: %2; }").arg(bg, textCol));
        frameFmt.setTopMargin(15);
        frameFmt.setBottomMargin(15);
        frameFmt.setRightMargin(20);
        frameFmt.setLeftMargin(editor->leftMarginPixels());
        doc->rootFrame()->setFrameFormat(frameFmt);
        doc->setPageSize(QSizeF(-1, -1));
        doc->setTextWidth(editor->viewport()->width());
        break;

    case WebLayout:
        editor->isPrintLayout = false;
        ruler->setVisible(false);
        editor->setStyleSheet(QString("EditorView { background-color: %1; color: %2; }").arg(bg, textCol));
        frameFmt.setTopMargin(30);
        frameFmt.setBottomMargin(30);
        frameFmt.setLeftMargin(30);
        frameFmt.setRightMargin(30);
        doc->rootFrame()->setFrameFormat(frameFmt);
        doc->setPageSize(QSizeF(-1, -1));
        doc->setTextWidth(editor->viewport()->width());
        break;

    case PrintLayout:
        editor->isPrintLayout = true;
        ruler->setVisible(true);
        editor->setStyleSheet(QString("EditorView { background-color: %1; }")
            .arg(darkModeActive ? "#121212" : "#707070"));
        frameFmt.setTopMargin(40);
        frameFmt.setBottomMargin(40);
        frameFmt.setLeftMargin(editor->leftMarginPixels() > 0 ? editor->leftMarginPixels() : 40);
        frameFmt.setRightMargin(40);
        doc->rootFrame()->setFrameFormat(frameFmt);
        doc->setPageSize(QSizeF(816, 1056));
        doc->setTextWidth(816);
        break;

    case CodeLayout:
        break;
    }

    doc->markContentsDirty(0, doc->characterCount());
    editor->viewport()->update();

    doc->setModified(wasModified);
    doc->blockSignals(docSignalsBlocked);
}

void MicroWriter::toggleDarkMode(bool dark) {
    darkModeActive = dark;
    QSettings("MicroWriterOrg", "MicroWriterApp").setValue("darkMode", dark);

    if (dark) {
        qApp->setStyleSheet(
            "QMainWindow { background-color: #2b2b2b; color: #ffffff; }"
            "QMenuBar, QMenu, QToolBar, QStatusBar { background-color: #3c3c3c; color: #ffffff; border: none; }"
            "QMenuBar::item:selected, QMenu::item:selected { background-color: #505050; }"
            "QTabWidget::pane { border: 1px solid #444444; }"
            "QTabBar::tab { background: #3c3c3c; color: #cccccc; padding: 6px 12px; border: 1px solid #444444; }"
            "QTabBar::tab:selected { background: #2b2b2b; color: #ffffff; border-bottom-color: #2b2b2b; }"
            "QPushButton { background-color: #444444; color: #ffffff; border: 1px solid #555555; padding: 4px; }"
            "QPushButton:hover { background-color: #555555; }"
            "QLineEdit, QComboBox, QSpinBox, QFontComboBox { background-color: #1e1e1e; color: #ffffff; border: 1px solid #555555; }"
        );
    } else {
        qApp->setStyleSheet("");
    }

    for (int i = 0; i < tabWidget->count(); ++i) {
        BaseEditor *ed = dynamic_cast<BaseEditor*>(tabWidget->widget(i));
        if (ed) {
            ed->setDarkMode(dark);
            if (EditorView *richEd = dynamic_cast<EditorView*>(ed)) {
                applyLayoutModeToEditor(richEd, currentLayoutMode);
            }
        }
    }
}

void MicroWriter::updateStats() {
    BaseEditor *editor = currentBaseEditor();
    if (!editor) return;

    EditorStats s = editor->getStats();
    const char *eolStr = (s.eol == EolCrlf) ? "CRLF" : (s.eol == EolCr) ? "CR" : "LF";

    statusLabel->setText(QString("Line: %1, Col: %2 | Words: %3 | Chars: %4 | Enc: %5 | EOL: %6")
        .arg(s.line).arg(s.column).arg(s.words).arg(s.characters).arg(s.encoding).arg(eolStr));
}

void MicroWriter::updateFormatToolbarState() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QTextCharFormat fmt = editor->currentCharFormat();
    if (fontCombo) {
        QSignalBlocker blocker(fontCombo);
        QStringList fams = fmt.fontFamilies().toStringList();
        fontCombo->setCurrentFont(fams.isEmpty() ? fmt.font() : QFont(fams.first()));
    }
    if (sizeSpin) {
        QSignalBlocker blocker(sizeSpin);
        int pt = qRound(fmt.fontPointSize() > 0 ? fmt.fontPointSize() : fmt.font().pointSize());
        if (pt > 0) sizeSpin->setValue(pt);
    }
}

void MicroWriter::updateWindowTitleAndStats() {
    BaseEditor *editor = currentBaseEditor();
    if (!editor) return;

    QString fPath = editor->filePath();
    setWindowTitle(fPath.isEmpty() ? "Qt6 MicroWriter - New File" : "Qt6 MicroWriter - " + fPath);
    updateStats();
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
    actionManager->setupDefaultToolBar(tb1);
}

void MicroWriter::createStatusBar() {
    statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(statusLabel);
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
        QStringList keys;
        for (QAction *act : tb->actions()) {
            if (!act->objectName().isEmpty()) keys.append(act->objectName());
        }
        settings.setValue("actions", keys);
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

    const QMap<QString, QAction*> &actMap = actionManager->actionMap();
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        QString name = settings.value("name").toString();
        QStringList keys = settings.value("actions").toStringList();

        QToolBar *tb = addToolBar(name);
        tb->setObjectName(name);
        tb->setWindowTitle(name);
        toolbars.append(tb);

        if (name == "Toolbar 1") {
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

        for (const QString &k : keys) {
            if (actMap.contains(k)) tb->addAction(actMap[k]);
        }
    }
    settings.endArray();
}

void MicroWriter::createNewToolbar() {
    bool ok;
    QString name = QInputDialog::getText(this, tr("New Toolbar"), tr("Toolbar Name:"), QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    QToolBar *tb = addToolBar(name);
    tb->setObjectName(name);
    tb->setWindowTitle(name);
    toolbars.append(tb);
}

void MicroWriter::closeEvent(QCloseEvent *event) {
    for (int i = 0; i < tabWidget->count(); ++i) {
        if (!maybeSaveTab(i)) {
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
        menu->addAction(tb->toggleViewAction());
    }
    return menu;
}
