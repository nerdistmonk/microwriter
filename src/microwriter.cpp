#include "microwriter.h"
#include "editorview.h"
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

EditorView* MicroWriter::currentEditor() const {
    return qobject_cast<EditorView*>(tabWidget->currentWidget());
}

void MicroWriter::fontfamily(const QFont &font) {
    DocumentFormatter::applyFontFamily(currentEditor(), font);
    updateFormatToolbarState();
}

void MicroWriter::fontSize(int size) {
    DocumentFormatter::applyFontSize(currentEditor(), size);
    updateFormatToolbarState();
}

void MicroWriter::openFilePath(const QString &fileName) {
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

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

    if (fileName.endsWith(".html") || fileName.endsWith(".htm")) editor->setHtml(content);
    else if (fileName.endsWith(".md")) editor->setMarkdown(content);
    else editor->setPlainText(content);

    editor->filePath = fileName;
    editor->document()->setModified(false);

    if (tabWidget->count() == 1) {
        EditorView *firstEd = qobject_cast<EditorView*>(tabWidget->widget(0));
        if (firstEd && firstEd->filePath.isEmpty() && !firstEd->document()->isModified() && firstEd->toPlainText().isEmpty()) {
            tabWidget->removeTab(0);
            delete firstEd;
        }
    }

    int index = tabWidget->addTab(editor, QFileInfo(fileName).fileName());
    tabWidget->setCurrentIndex(index);
    ruler->setEditor(editor);
    applyLayoutModeToEditor(editor, currentLayoutMode);
    updateFormatToolbarState();
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
    QString fileName = QFileDialog::getOpenFileName(this, "Open Document", "", "Documents (*.html *.htm *.md *.txt);;All Files (*.*)", nullptr, QFileDialog::DontUseNativeDialog);
    if (!fileName.isEmpty()) openFilePath(fileName);
}

void MicroWriter::saveFile() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    if (editor->filePath.isEmpty()) saveAsFile();
    else writeFile(editor->filePath);
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
            if (selectedFilter.contains("*.md")) fileName += ".md";
            else if (selectedFilter.contains("*.txt")) fileName += ".txt";
            else fileName += ".html";
        }

        editor->filePath = fileName;
        writeFile(editor->filePath);
        tabWidget->setTabText(tabWidget->currentIndex(), QFileInfo(fileName).fileName());
        updateWindowTitleAndStats();
    }
}

void MicroWriter::writeFile(const QString &fileName) {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QString content;
    if (fileName.endsWith(".md")) content = editor->toMarkdown();
    else if (fileName.endsWith(".html") || fileName.endsWith(".htm")) {
        content = editor->toHtml();
        content.replace(QRegularExpression("<!DOCTYPE[^>]*>", QRegularExpression::CaseInsensitiveOption), "<!DOCTYPE html>");
    } else content = editor->toPlainText();

    content.replace(QRegularExpression("\\r\\n|\\r"), "\n");
    if (editor->eolFormat == EolCrlf) content.replace('\n', "\r\n");
    else if (editor->eolFormat == EolCr) content.replace('\n', "\r");

    QTextStream out(&file);
    out.setEncoding(QStringConverter::encodingForName(editor->encodingName.toUtf8()).value_or(QStringConverter::Utf8));
    out << content;
    editor->document()->setModified(false);
}

void MicroWriter::closeTabRequested(int index) {
    EditorView *editor = qobject_cast<EditorView*>(tabWidget->widget(index));
    if (editor && maybeSave(editor)) {
        tabWidget->removeTab(index);
        delete editor;
        if (tabWidget->count() == 0) newFile();
    }
}

bool MicroWriter::maybeSave(EditorView *editor) {
    if (!editor || !editor->document()->isModified()) return true;

    QString name = editor->filePath.isEmpty() ? "Untitled" : QFileInfo(editor->filePath).fileName();
    QMessageBox::StandardButton ret = QMessageBox::warning(this, "MicroWriter",
        tr("The document '%1' has been modified.\nDo you want to save your changes?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save) {
        tabWidget->setCurrentWidget(editor);
        saveFile();
        return !editor->document()->isModified();
    }
    return ret != QMessageBox::Cancel;
}

void MicroWriter::setLayoutMode(LayoutMode mode) {
    currentLayoutMode = mode;
    for (int i = 0; i < tabWidget->count(); ++i) {
        applyLayoutModeToEditor(qobject_cast<EditorView*>(tabWidget->widget(i)), mode);
    }
}

void MicroWriter::applyLayoutModeToEditor(EditorView *editor, LayoutMode mode) {
    if (!editor) return;
    QTextDocument *doc = editor->document();
    QString bg = darkModeActive ? "#1e1e1e" : "#ffffff";
    QString textCol = darkModeActive ? "#dcdcdc" : "#000000";

    switch (mode) {
    case NormalLayout:
        editor->isPrintLayout = false;
        ruler->setVisible(true);
        editor->setStyleSheet(QString("QTextEdit { background-color: %1; color: %2; padding: 15px; }").arg(bg, textCol));
        doc->setPageSize(QSizeF());
        break;
    case WebLayout:
        editor->isPrintLayout = false;
        ruler->setVisible(false);
        editor->setStyleSheet(QString("QTextEdit { background-color: %1; color: %2; padding: 30px; }").arg(bg, textCol));
        doc->setPageSize(QSizeF());
        break;
    case PrintLayout:
        editor->isPrintLayout = true;
        ruler->setVisible(true);
        editor->setStyleSheet(QString("EditorView { background-color: %1; padding: 30px; } EditorView QTextEdit { background-color: %2; color: %3; border: 1px solid #404040; }")
            .arg(darkModeActive ? "#121212" : "#707070", darkModeActive ? "#2b2b2b" : "#ffffff", textCol));
        doc->setPageSize(QSizeF(816, 1056));
        break;
    }
    editor->viewport()->update();
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
    setLayoutMode(currentLayoutMode);
}

void MicroWriter::updateStats() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QString text = editor->toPlainText();
    int words = text.trimmed().isEmpty() ? 0 : text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
    QTextCursor cursor = editor->textCursor();

    const char *eol = (editor->eolFormat == EolCrlf) ? "CRLF" : (editor->eolFormat == EolCr ? "CR" : "LF");
    statusLabel->setText(QString("Line: %1, Col: %2 | Words: %3 | Chars: %4 | Enc: %5 | EOL: %6")
        .arg(cursor.blockNumber() + 1).arg(cursor.columnNumber() + 1).arg(words).arg(text.length()).arg(editor->encodingName).arg(eol));
}

void MicroWriter::updateFormatToolbarState() {
    EditorView *editor = currentEditor();
    if (!editor) return;

    QTextCharFormat fmt = editor->currentCharFormat();
    if (fontCombo) {
        fontCombo->blockSignals(true);
        QStringList fams = fmt.fontFamilies().toStringList();
        fontCombo->setCurrentFont(fams.isEmpty() ? fmt.font() : QFont(fams.first()));
        fontCombo->blockSignals(false);
    }
    if (sizeSpin) {
        sizeSpin->blockSignals(true);
        int pt = qRound(fmt.fontPointSize() > 0 ? fmt.fontPointSize() : fmt.font().pointSize());
        if (pt > 0) sizeSpin->setValue(pt);
        sizeSpin->blockSignals(false);
    }
}

void MicroWriter::updateWindowTitleAndStats() {
    EditorView *editor = currentEditor();
    if (!editor) return;
    setWindowTitle(editor->filePath.isEmpty() ? "Qt6 MicroWriter - New File" : "Qt6 MicroWriter - " + editor->filePath);
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

        for (const QString &k : keys) {
            if (actMap.contains(k)) tb->addAction(actMap[k]);
        }
    }
    settings.endArray();

    if (settings.contains("geometry")) restoreGeometry(settings.value("geometry").toByteArray());
    if (settings.contains("windowState")) restoreState(settings.value("windowState").toByteArray());
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
            ToolbarCustomizeDialog dlg(tb, actionManager->allActions(), this);
            dlg.exec();
            saveToolbarSettings();
        });
        menu->addSeparator();
    }

    QAction *newTbAct = menu->addAction(tr("Create New Toolbar..."));
    connect(newTbAct, &QAction::triggered, this, &MicroWriter::createNewToolbar);
    return menu;
}

void MicroWriter::createNewToolbar() {
    bool ok;
    QString name = QInputDialog::getText(this, tr("Create New Toolbar"), tr("Toolbar Name:"),
        QLineEdit::Normal, QString("Toolbar %1").arg(toolbars.size() + 1), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        QToolBar *tb = addToolBar(name.trimmed());
        tb->setObjectName(name.trimmed());
        tb->setWindowTitle(name.trimmed());
        toolbars.append(tb);

        ToolbarCustomizeDialog dlg(tb, actionManager->allActions(), this);
        dlg.exec();
        saveToolbarSettings();
    }
}
