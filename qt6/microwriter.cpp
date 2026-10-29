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
#include <QWidgetAction>
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
#include <QSet>

static const QSet<QString> g_codeExtensions = {
    "cpp", "c", "h", "hpp", "cc", "cxx", "py", "js", "ts", "json",
    "css", "xml", "sh", "bash", "ini", "cfg", "conf", "yaml", "yml", "cmake", "txt"
};

static const QSet<QString> g_strictCodeLockExtensions = {
    "cpp", "c", "h", "hpp", "cc", "cxx", "py", "js", "ts", "json",
    "css", "xml", "sh", "bash", "ini", "cfg", "conf", "yaml", "yml", "cmake"
};

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
      darkAct(nullptr),
      m_codeToolbarsActive(false)
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
        switchToolbarProfile(!isRich);

        if (fontCombo) fontCombo->setEnabled(isRich);
        if (sizeSpin) sizeSpin->setEnabled(isRich);

        if (isRich) {
            EditorView *ed = dynamic_cast<EditorView*>(base);
            LayoutMode targetMode = ed->layoutMode();
            if (targetMode == CodeLayout) {
                targetMode = NormalLayout;
                ed->setLayoutMode(NormalLayout);
            }
            currentLayoutMode = targetMode;

            ruler->setEditor(ed);
            applyLayoutModeToEditor(ed, currentLayoutMode);
            updateFormatToolbarState();
        } else {
            currentLayoutMode = CodeLayout;
            ruler->setVisible(false);
        }

        if (actionManager) {
            actionManager->syncLayoutModeAction(currentLayoutMode);
            actionManager->setFormatMenuVisible(isRich);
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
        if (tabWidget->currentWidget() == w) {
            updateWindowTitleAndStats();
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

void MicroWriter::nextTab() {
    int count = tabWidget->count();
    if (count <= 1) return;
    int next = (tabWidget->currentIndex() + 1) % count;
    tabWidget->setCurrentIndex(next);
}

void MicroWriter::openFilePath(const QString &fileName) {
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return;

    QByteArray rawData = file.readAll();
    EolFormat eol = EolLf;
    if (rawData.contains("\r\n")) eol = EolCrlf;
    else if (rawData.contains('\r') && !rawData.contains('\n')) eol = EolCr;

    QString encName = "UTF-8";
    QTextStream in(&rawData);
    in.setEncoding(QStringConverter::encodingForName(encName.toUtf8()).value_or(QStringConverter::Utf8));
    QString content = in.readAll();

    QString ext = QFileInfo(fileName).suffix().toLower();

    LayoutMode targetMode = currentLayoutMode;
    if (g_codeExtensions.contains(ext)) {
        targetMode = CodeLayout;
    }

    currentLayoutMode = targetMode;
    switchToolbarProfile(targetMode == CodeLayout);

    if (actionManager) {
        actionManager->syncLayoutModeAction(targetMode);
        actionManager->setFormatMenuVisible(targetMode != CodeLayout);
    }

    BaseEditor *newBase = nullptr;
    QWidget *newWidget = nullptr;

    if (targetMode == CodeLayout) {
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
        applyLayoutModeToEditor(editor, targetMode);

        newBase = editor;
        newWidget = editor;
        ruler->setEditor(editor);
        if (fontCombo) fontCombo->setEnabled(true);
        if (sizeSpin) sizeSpin->setEnabled(true);
    }

    attachEditorSignals(newBase);

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
    if (targetMode != CodeLayout) {
        updateFormatToolbarState();
    }

    newBase->setModified(false);
    tabWidget->setTabText(index, QFileInfo(fileName).fileName());
}

void MicroWriter::newFile(const QString &title) {
    BaseEditor *newBase = nullptr;
    QWidget *newWidget = nullptr;

    switchToolbarProfile(currentLayoutMode == CodeLayout);

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

    newBase->setModified(false);
    tabWidget->setTabText(index, title);

    if (actionManager) {
        actionManager->setFormatMenuVisible(currentLayoutMode != CodeLayout);
    }
}

void MicroWriter::openFile() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open Document",
        "",
        "Supported Files (*.html *.htm *.md *.txt *.cpp *.c *.h *.hpp *.py *.js *.json *.css *.xml *.ini *.cfg *.conf *.sh);;"
        "Documents (*.html *.htm *.md);;"
        "Source Code (*.cpp *.c *.h *.hpp *.cc *.cxx *.py *.js *.ts *.json *.css *.xml *.sh *.bash *.ini *.cfg *.conf *.yaml *.yml *.cmake *.txt);;"
        "All Files (*.*)",
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
    dialog.setNameFilters({
        "HTML Files (*.html *.htm)",
        "Markdown (*.md)",
        "Text Files (*.txt)",
        "C/C++ Source (*.cpp *.c *.h *.hpp)",
        "All Files (*.*)"
    });

    if (dialog.exec() == QDialog::Accepted) {
        QString fileName = dialog.selectedFiles().first();
        QString selectedFilter = dialog.selectedNameFilter();

        QFileInfo fileInfo(fileName);
        if (fileInfo.suffix().isEmpty()) {
            if (selectedFilter.contains("*.md")) fileName += ".md";
            else if (selectedFilter.contains("*.txt")) fileName += ".txt";
            else if (selectedFilter.contains("*.cpp")) fileName += ".cpp";
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
    if (!file.open(QIODevice::WriteOnly)) return;

    QString content = editor->getContentForSave();

    content.replace(QRegularExpression("\\r\\n|\\r"), "\n");
    if (editor->eolFormat() == EolCrlf) content.replace('\n', "\r\n");
    else if (editor->eolFormat() == EolCr) content.replace('\n', "\r");

    QTextStream out(&file);
    out.setEncoding(QStringConverter::encodingForName(editor->encodingName().toUtf8()).value_or(QStringConverter::Utf8));
    out << content;
    editor->setModified(false);

    int curIdx = tabWidget->currentIndex();
    if (curIdx != -1) {
        tabWidget->setTabText(curIdx, QFileInfo(fileName).fileName());
    }
    updateWindowTitleAndStats();
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
    int curIdx = tabWidget->currentIndex();
    if (curIdx == -1) return;

    BaseEditor *curBase = currentBaseEditor();
    if (!curBase) return;

    // Lock-out guard: Prevent source code and configuration files from ever switching out of CodeLayout
    QString ext = QFileInfo(curBase->filePath()).suffix().toLower();
    if (g_strictCodeLockExtensions.contains(ext) && mode != CodeLayout) {
        if (actionManager) {
            actionManager->syncLayoutModeAction(CodeLayout);
        }
        return;
    }

    currentLayoutMode = mode;
    switchToolbarProfile(mode == CodeLayout);

    if (actionManager) {
        actionManager->syncLayoutModeAction(mode);
        actionManager->setFormatMenuVisible(mode != CodeLayout);
    }

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
    if (mode != CodeLayout) {
        editor->setLayoutMode(mode);
    }
    QTextDocument *doc = editor->document();
    bool wasModified = doc->isModified();

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
        frameFmt.setRightMargin(editor->rightMarginPixels() > 0 ? editor->rightMarginPixels() : 20);
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
        frameFmt.setRightMargin(editor->rightMarginPixels() > 0 ? editor->rightMarginPixels() : 40);
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
            "QPushButton { background-color: #4a4a4a; color: #ffffff; border: 1px solid #555555; padding: 4px 8px; border-radius: 3px; }"
            "QPushButton:hover { background-color: #5a5a5a; }"
            "QLineEdit, QSpinBox, QComboBox { background-color: #333333; color: #ffffff; border: 1px solid #555555; padding: 3px; }"
        );
    } else {
        qApp->setStyleSheet("");
    }

    for (int i = 0; i < tabWidget->count(); ++i) {
        BaseEditor *base = dynamic_cast<BaseEditor*>(tabWidget->widget(i));
        if (base) {
            base->setDarkMode(dark);
            if (EditorView *ed = dynamic_cast<EditorView*>(base)) {
                applyLayoutModeToEditor(ed, ed->layoutMode());
            }
        }
    }
}

void MicroWriter::updateFormatToolbarState() {
    BaseEditor *base = currentBaseEditor();
    if (!base || !base->supportsRichFormatting()) {
        if (fontCombo) fontCombo->setEnabled(false);
        if (sizeSpin) sizeSpin->setEnabled(false);
        return;
    }

    if (fontCombo) fontCombo->setEnabled(true);
    if (sizeSpin) sizeSpin->setEnabled(true);

    EditorView *ed = dynamic_cast<EditorView*>(base);
    if (!ed) return;

    QTextCursor cursor = ed->textCursor();
    QTextCharFormat fmt = cursor.charFormat();

    if (fontCombo) {
        QSignalBlocker b(fontCombo);
        fontCombo->setCurrentFont(fmt.font());
    }
    if (sizeSpin) {
        QSignalBlocker b(sizeSpin);
        sizeSpin->setValue(fmt.font().pointSize() > 0 ? fmt.font().pointSize() : 12);
    }
}

void MicroWriter::updateStats() {
    updateWindowTitleAndStats();
}

void MicroWriter::updateWindowTitleAndStats() {
    BaseEditor *base = currentBaseEditor();
    if (!base) {
        setWindowTitle("MicroWriter");
        if (statusLabel) statusLabel->setText("");
        return;
    }

    QString fname = base->filePath().isEmpty() ? "Untitled" : QFileInfo(base->filePath()).fileName();
    if (base->isModified()) fname += " *";
    setWindowTitle(QString("%1 - MicroWriter").arg(fname));

    if (statusLabel) {
        EditorStats s = base->getStats();
        QString eolStr = "LF";
        if (s.eol == EolCrlf) eolStr = "CRLF";
        else if (s.eol == EolCr) eolStr = "CR";

        statusLabel->setText(QString("Lines: %1 | Cols: %2 | Words: %3 | Chars: %4 | %5 | %6")
            .arg(s.line)
            .arg(s.column)
            .arg(s.words)
            .arg(s.characters)
            .arg(s.encoding)
            .arg(eolStr));
    }
}

void MicroWriter::createStatusBar() {
    statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(statusLabel);
}

void MicroWriter::clearActiveToolbars() {
    for (QToolBar *tb : toolbars) {
        removeToolBar(tb);
        delete tb;
    }
    toolbars.clear();
    fontCombo = nullptr;
    sizeSpin = nullptr;
}

void MicroWriter::buildDefaultRichToolbars() {
    QToolBar *mainToolBar = addToolBar(tr("Toolbar 1"));
    mainToolBar->setObjectName("Toolbar 1");
    toolbars.append(mainToolBar);

    fontCombo = new QFontComboBox(this);
    connect(fontCombo, &QFontComboBox::currentFontChanged, this, &MicroWriter::fontfamily);
    mainToolBar->addWidget(fontCombo);

    sizeSpin = new QSpinBox(this);
    sizeSpin->setRange(6, 96);
    sizeSpin->setValue(12);
    connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MicroWriter::fontSize);
    mainToolBar->addWidget(sizeSpin);

    mainToolBar->addSeparator();

    if (actionManager) {
        actionManager->setupDefaultToolBar(mainToolBar);
    }
}

void MicroWriter::buildDefaultCodeToolbars() {
    QToolBar *codeBar = addToolBar(tr("Toolbar 1"));
    codeBar->setObjectName("Toolbar 1");
    toolbars.append(codeBar);

    if (actionManager) {
        const QMap<QString, QAction*> &map = actionManager->actionMap();
        QStringList defaultCodeActions = {"New", "Open...", "Save", "Undo", "Redo", "Find & Replace...", "Indent", "Unindent"};
        for (const QString &actName : defaultCodeActions) {
            if (map.contains(actName)) {
                codeBar->addAction(map[actName]);
            }
        }
    }
}

void MicroWriter::switchToolbarProfile(bool toCode) {
    if (m_codeToolbarsActive == toCode && !toolbars.isEmpty()) return;

    saveToolbarSettings();
    clearActiveToolbars();

    m_codeToolbarsActive = toCode;
    loadToolbarSettings();
}

void MicroWriter::loadToolbarSettings() {
    QSettings settings("MicroWriterOrg", "MicroWriterApp");
    QString group = m_codeToolbarsActive ? "Toolbars_Code" : "Toolbars_Rich";

    settings.beginGroup(group);
    int count = settings.beginReadArray("list");

    if (count == 0) {
        settings.endArray();
        settings.endGroup();
        if (m_codeToolbarsActive) {
            buildDefaultCodeToolbars();
        } else {
            buildDefaultRichToolbars();
        }
        return;
    }

    const QMap<QString, QAction*> &map = actionManager ? actionManager->actionMap() : QMap<QString, QAction*>();
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        QString tbName = settings.value("name").toString();
        bool hasFont = settings.value("hasFontCombo", false).toBool();
        bool hasSize = settings.value("hasSizeSpin", false).toBool();
        QStringList actList = settings.value("actions").toStringList();

        QToolBar *tb = addToolBar(tbName);
        tb->setObjectName(tbName);
        toolbars.append(tb);

        if (hasFont && !m_codeToolbarsActive) {
            fontCombo = new QFontComboBox(this);
            connect(fontCombo, &QFontComboBox::currentFontChanged, this, &MicroWriter::fontfamily);
            tb->addWidget(fontCombo);
        }
        if (hasSize && !m_codeToolbarsActive) {
            sizeSpin = new QSpinBox(this);
            sizeSpin->setRange(6, 96);
            sizeSpin->setValue(12);
            connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MicroWriter::fontSize);
            tb->addWidget(sizeSpin);
        }

        for (const QString &actKey : actList) {
            if (map.contains(actKey)) {
                tb->addAction(map[actKey]);
            }
        }
    }

    settings.endArray();
    QByteArray stateData = settings.value("windowState").toByteArray();
    if (!stateData.isEmpty()) {
        restoreState(stateData);
    }
    settings.endGroup();
}

void MicroWriter::saveToolbarSettings() {
    if (toolbars.isEmpty()) return;

    QSettings settings("MicroWriterOrg", "MicroWriterApp");
    QString group = m_codeToolbarsActive ? "Toolbars_Code" : "Toolbars_Rich";

    settings.beginGroup(group);
    settings.setValue("windowState", saveState());

    settings.beginWriteArray("list");
    for (int i = 0; i < toolbars.size(); ++i) {
        settings.setArrayIndex(i);
        QToolBar *tb = toolbars[i];
        settings.setValue("name", tb->objectName().isEmpty() ? tb->windowTitle() : tb->objectName());

        bool hasFont = false;
        bool hasSize = false;
        QStringList acts;

        for (QAction *a : tb->actions()) {
            QWidgetAction *wa = dynamic_cast<QWidgetAction*>(a);
            if (wa) {
                if (fontCombo && wa->defaultWidget() == fontCombo) hasFont = true;
                else if (sizeSpin && wa->defaultWidget() == sizeSpin) hasSize = true;
            } else if (!a->objectName().isEmpty()) {
                acts.append(a->objectName());
            }
        }

        settings.setValue("hasFontCombo", hasFont);
        settings.setValue("hasSizeSpin", hasSize);
        settings.setValue("actions", acts);
    }
    settings.endArray();
    settings.endGroup();
}

void MicroWriter::createNewToolbar() {
    bool ok;
    QString name = QInputDialog::getText(this, tr("Create Toolbar"), tr("Toolbar Name:"), QLineEdit::Normal, "", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        QString clean = name.trimmed();
        QToolBar *tb = addToolBar(clean);
        tb->setObjectName(clean);
        toolbars.append(tb);
        addToolBarBreak();
    }
}

void MicroWriter::customizeToolbar(QToolBar *toolbar) {
    if (!toolbar && !toolbars.isEmpty()) {
        toolbar = toolbars.first();
    }
    if (!toolbar || !actionManager) return;

    ToolbarCustomizeDialog dlg(toolbar, actionManager->allMenuActions(), this);
    if (dlg.exec() == QDialog::Accepted) {
        saveToolbarSettings();
    }
}

void MicroWriter::removeToolbar(QToolBar *toolbar) {
    if (!toolbar) return;
    if (toolbars.size() <= 1) {
        QMessageBox::information(this, tr("Cannot Remove"), tr("You must keep at least one toolbar."));
        return;
    }

    toolbars.removeAll(toolbar);
    removeToolBar(toolbar);
    delete toolbar;
    saveToolbarSettings();
}

QMenu* MicroWriter::createPopupMenu() {
    QMenu *menu = new QMenu(this);

    QPoint globalPos = QCursor::pos();
    QWidget *wUnder = QApplication::widgetAt(globalPos);
    QToolBar *clickedBar = nullptr;
    while (wUnder) {
        if (QToolBar *tb = qobject_cast<QToolBar*>(wUnder)) {
            clickedBar = tb;
            break;
        }
        wUnder = wUnder->parentWidget();
    }

    if (clickedBar) {
        QAction *custAct = menu->addAction(tr("Customize '%1'...").arg(clickedBar->windowTitle()));
        connect(custAct, &QAction::triggered, this, [this, clickedBar]() {
            customizeToolbar(clickedBar);
        });

        QAction *remAct = menu->addAction(tr("Remove '%1'").arg(clickedBar->windowTitle()));
        connect(remAct, &QAction::triggered, this, [this, clickedBar]() {
            removeToolbar(clickedBar);
        });
        menu->addSeparator();
    }

    QAction *newTb = menu->addAction(tr("New Toolbar..."));
    connect(newTb, &QAction::triggered, this, &MicroWriter::createNewToolbar);

    menu->addSeparator();
    for (QToolBar *tb : toolbars) {
        menu->addAction(tb->toggleViewAction());
    }

    return menu;
}

void MicroWriter::closeEvent(QCloseEvent *event) {
    for (int i = tabWidget->count() - 1; i >= 0; --i) {
        tabWidget->setCurrentIndex(i);
        if (!maybeSaveTab(i)) {
            event->ignore();
            return;
        }
    }
    saveToolbarSettings();
    QSettings settings("MicroWriterOrg", "MicroWriterApp");
    settings.setValue("geometry", saveGeometry());
    event->accept();
}