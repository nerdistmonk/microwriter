#include "actionmanager.h"
#include "microwriter.h"
#include "editorview.h"
#include "documentformatter.h"
#include "documentexporter.h"
#include "texttools.h"
#include "dialogs.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QToolBar>
#include <QInputDialog>

ActionManager::ActionManager(MicroWriter *window)
    : QObject(window), m_window(window), m_formatMenu(nullptr), m_layoutGrp(nullptr)
{
}

void ActionManager::registerAction(QAction *act) {
    if (act && !act->text().isEmpty()) {
        m_allMenuActions.append(act);
        QString key = act->text();
        key.remove('&');
        key = key.trimmed();
        act->setObjectName(key);
        m_actionMap[key] = act;
    }
}

void ActionManager::syncLayoutModeAction(LayoutMode mode) {
    if (m_layoutActions.contains(mode)) {
        m_layoutActions[mode]->setChecked(true);
    }
}

void ActionManager::setFormatMenuVisible(bool visible) {
    if (m_formatMenu) {
        m_formatMenu->menuAction()->setVisible(visible);
    }
}

void ActionManager::setupMenus(QMenuBar *menuBar) {
    // --- File Menu ---
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));

    auto addFileAct = [&](const QString &title, const QKeySequence &seq, auto slot) {
        QAction *act = new QAction(title, m_window);
        if (!seq.isEmpty()) act->setShortcut(seq);
        connect(act, &QAction::triggered, m_window, slot);
        fileMenu->addAction(act);
        registerAction(act);
    };

    addFileAct(tr("&New"), QKeySequence::New, [this](){ m_window->newFile(); });
    addFileAct(tr("&Open..."), QKeySequence::Open, &MicroWriter::openFile);
    addFileAct(tr("&Save"), QKeySequence::Save, &MicroWriter::saveFile);
    addFileAct(tr("Save &As..."), QKeySequence::SaveAs, &MicroWriter::saveAsFile);

    fileMenu->addSeparator();

    QMenu *encMenu = fileMenu->addMenu(tr("Text &Encoding"));
    QActionGroup *encGrp = new QActionGroup(m_window);
    for (const QString &enc : {"UTF-8", "UTF-16", "ISO-8859-1", "Windows-1252"}) {
        QAction *act = new QAction(enc, m_window);
        act->setCheckable(true);
        encGrp->addAction(act);
        if (enc == "UTF-8") act->setChecked(true);
        encMenu->addAction(act);
        connect(act, &QAction::triggered, this, [this, enc](){
            if (m_window->currentBaseEditor()) {
                m_window->currentBaseEditor()->setEncodingName(enc);
                m_window->updateFormatToolbarState();
            }
        });
    }

    QMenu *eolMenu = fileMenu->addMenu(tr("&Line Endings (EOL)"));
    QActionGroup *eolGrp = new QActionGroup(m_window);
    auto addEol = [&](const QString &label, EolFormat eol, bool check) {
        QAction *act = new QAction(label, m_window);
        act->setCheckable(true);
        eolGrp->addAction(act);
        if (check) act->setChecked(true);
        eolMenu->addAction(act);
        connect(act, &QAction::triggered, this, [this, eol](){
            if (m_window->currentBaseEditor()) {
                m_window->currentBaseEditor()->setEolFormat(eol);
                m_window->updateFormatToolbarState();
            }
        });
    };
    addEol(tr("Unix (LF / \\n)"), EolLf, true);
    addEol(tr("Windows (CRLF / \\r\\n)"), EolCrlf, false);
    addEol(tr("Mac Classic (CR / \\r)"), EolCr, false);

    fileMenu->addSeparator();

    auto addExportAct = [&](const QString &title, const QKeySequence &seq, auto fn) {
        QAction *act = new QAction(title, m_window);
        if (!seq.isEmpty()) act->setShortcut(seq);
        connect(act, &QAction::triggered, this, [this, fn](){
            EditorView *ed = m_window->currentEditor();
            if (ed) fn(ed, m_window);
        });
        fileMenu->addAction(act);
        registerAction(act);
    };

    addExportAct(tr("Print Pre&view..."), QKeySequence(), DocumentExporter::printPreview);
    addExportAct(tr("&Print..."), QKeySequence::Print, DocumentExporter::printDocument);
    addExportAct(tr("Export as &PDF..."), QKeySequence(), DocumentExporter::exportPdf);
    addExportAct(tr("Export as &ODT..."), QKeySequence(), DocumentExporter::exportOdt);

    fileMenu->addSeparator();
    addFileAct(tr("E&xit"), QKeySequence::Quit, &QWidget::close);

    // --- Edit Menu ---
    QMenu *editMenu = menuBar->addMenu(tr("&Edit"));
    auto addEditAct = [&](const QString &title, const QKeySequence &seq, auto fn) {
        QAction *act = new QAction(title, m_window);
        act->setShortcut(seq);
        connect(act, &QAction::triggered, this, [this, fn](){
            if (m_window->currentBaseEditor()) (m_window->currentBaseEditor()->*fn)();
        });
        editMenu->addAction(act);
        registerAction(act);
    };

    addEditAct(tr("&Undo"), QKeySequence::Undo, &BaseEditor::undo);
    addEditAct(tr("&Redo"), QKeySequence::Redo, &BaseEditor::redo);

    editMenu->addSeparator();
    QAction *findAct = new QAction(tr("&Find & Replace..."), m_window);
    findAct->setShortcut(QKeySequence::Find);
    connect(findAct, &QAction::triggered, this, [this](){
        BaseEditor *base = m_window->currentBaseEditor();
        if (!base) return;
        FindReplaceDialog *dlg = new FindReplaceDialog(base, m_window);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    editMenu->addAction(findAct);
    registerAction(findAct);

    // --- Format Menu ---
    m_formatMenu = menuBar->addMenu(tr("&Format"));
    auto addFmt = [&](const QString &title, const QKeySequence &seq, auto fn) {
        QAction *act = new QAction(title, m_window);
        if (!seq.isEmpty()) act->setShortcut(seq);
        connect(act, &QAction::triggered, this, [this, fn](){
            EditorView *ed = m_window->currentEditor();
            if (!ed) return;
            fn(ed);
            m_window->updateFormatToolbarState();
        });
        m_formatMenu->addAction(act);
        registerAction(act);
    };

    addFmt(tr("&Bold"), QKeySequence::Bold, DocumentFormatter::toggleBold);
    addFmt(tr("&Italic"), QKeySequence::Italic, DocumentFormatter::toggleItalic);
    addFmt(tr("&Underline"), QKeySequence::Underline, DocumentFormatter::toggleUnderline);
    addFmt(tr("Strikethrough"), QKeySequence(), DocumentFormatter::toggleStrikethrough);
    addFmt(tr("Superscript"), QKeySequence(), DocumentFormatter::toggleSuperscript);
    addFmt(tr("Subscript"), QKeySequence(), DocumentFormatter::toggleSubscript);

    m_formatMenu->addSeparator();
    auto addFmtDlg = [&](const QString &title, auto fn) {
        QAction *act = new QAction(title, m_window);
        connect(act, &QAction::triggered, this, [this, fn](){
            EditorView *ed = m_window->currentEditor();
            if (!ed) return;
            fn(ed, m_window);
            m_window->updateFormatToolbarState();
        });
        m_formatMenu->addAction(act);
        registerAction(act);
    };

    addFmtDlg(tr("Text &Color..."), DocumentFormatter::applyTextColor);
    addFmtDlg(tr("Text &Shading..."), DocumentFormatter::applyTextShading);

    m_formatMenu->addSeparator();
    auto addAlign = [&](const QString &title, Qt::Alignment align) {
        QAction *act = new QAction(title, m_window);
        connect(act, &QAction::triggered, this, [this, align](){
            EditorView *ed = m_window->currentEditor();
            if (!ed) return;
            DocumentFormatter::setAlignment(ed, align);
        });
        m_formatMenu->addAction(act);
        registerAction(act);
    };
    addAlign(tr("Align &Left"), Qt::AlignLeft);
    addAlign(tr("Align &Center"), Qt::AlignCenter);
    addAlign(tr("Align &Right"), Qt::AlignRight);
    addAlign(tr("&Justify"), Qt::AlignJustify);

    m_formatMenu->addSeparator();
    QAction *doubleSpace = new QAction(tr("Double Spacing"), m_window);
    doubleSpace->setCheckable(true);
    connect(doubleSpace, &QAction::triggered, this, [this](bool checked){
        EditorView *ed = m_window->currentEditor();
        if (ed) DocumentFormatter::toggleDoubleSpacing(ed, checked);
    });
    m_formatMenu->addAction(doubleSpace);
    registerAction(doubleSpace);

    m_formatMenu->addSeparator();
    addFmt(tr("Bullet &List"), QKeySequence(), DocumentFormatter::insertBulletList);
    addFmt(tr("&Numbered List"), QKeySequence(), DocumentFormatter::insertNumberedList);
    addFmt(tr("Blockquote"), QKeySequence(), DocumentFormatter::insertBlockquote);
    addFmt(tr("Horizontal &Line"), QKeySequence(), DocumentFormatter::insertHorizontalLine);

    addFmtDlg(tr("Insert &Table..."), DocumentFormatter::insertTable);
    addFmtDlg(tr("Insert &Date & Time..."), DocumentFormatter::insertDateTime);
    addFmtDlg(tr("Insert &Link..."), DocumentFormatter::insertLink);
    addFmtDlg(tr("Insert &Symbol..."), DocumentFormatter::insertSymbol);

    m_formatMenu->addSeparator();
    addFmtDlg(tr("Insert &Image..."), DocumentFormatter::insertImage);
    addFmtDlg(tr("Header & Footer..."), DocumentFormatter::configureHeaderFooter);

    // --- Tools Menu ---
    QMenu *toolsMenu = menuBar->addMenu(tr("&Tools"));
    auto addTool = [&](const QString &title, auto fn) {
        QAction *act = new QAction(title, m_window);
        connect(act, &QAction::triggered, this, [this, fn](){
            BaseEditor *base = m_window->currentBaseEditor();
            if (base) fn(base);
        });
        toolsMenu->addAction(act);
        registerAction(act);
    };

    auto addBaseTool = [&](const QString &title, const QKeySequence &seq, auto fn) {
        QAction *act = new QAction(title, m_window);
        if (!seq.isEmpty()) act->setShortcut(seq);
        connect(act, &QAction::triggered, this, [this, fn](){
            BaseEditor *base = m_window->currentBaseEditor();
            if (base) fn(base);
        });
        toolsMenu->addAction(act);
        registerAction(act);
    };

    addBaseTool(tr("Indent"), QKeySequence(), TextTools::indent);
    addBaseTool(tr("Unindent"), QKeySequence(), TextTools::unindent);
    addBaseTool(tr("Strip Blank Lines"), QKeySequence(), TextTools::stripBlankLines);

    toolsMenu->addSeparator();
    addTool(tr("Sort Lines (Ignore Case)"), TextTools::sortLines);
    addTool(tr("Reverse Lines"), TextTools::reverseLines);

    toolsMenu->addSeparator();
    QMenu *caseMenu = toolsMenu->addMenu(tr("Fix Case"));
    auto addCase = [&](const QString &title, auto fn) {
        QAction *act = new QAction(title, m_window);
        connect(act, &QAction::triggered, this, [this, fn](){
            BaseEditor *base = m_window->currentBaseEditor();
            if (base) fn(base);
        });
        caseMenu->addAction(act);
        registerAction(act);
    };
    addCase(tr("UPPERCASE"), TextTools::toUppercase);
    addCase(tr("lowercase"), TextTools::toLowercase);
    addCase(tr("Title Case"), TextTools::toTitleCase);

    toolsMenu->addSeparator();
    QAction *pfxAct = new QAction(tr("Add Prefix..."), m_window);
    connect(pfxAct, &QAction::triggered, this, [this](){
        BaseEditor *base = m_window->currentBaseEditor();
        if (!base) return;
        bool ok;
        QString p = QInputDialog::getText(m_window, "Add Prefix", "Enter prefix string:", QLineEdit::Normal, "", &ok);
        if (ok) TextTools::addPrefix(base, p);
    });
    toolsMenu->addAction(pfxAct);
    registerAction(pfxAct);

    QAction *sfxAct = new QAction(tr("Add Suffix..."), m_window);
    connect(sfxAct, &QAction::triggered, this, [this](){
        BaseEditor *base = m_window->currentBaseEditor();
        if (!base) return;
        bool ok;
        QString s = QInputDialog::getText(m_window, "Add Suffix", "Enter suffix string:", QLineEdit::Normal, "", &ok);
        if (ok) TextTools::addSuffix(base, s);
    });
    toolsMenu->addAction(sfxAct);
    registerAction(sfxAct);

    toolsMenu->addSeparator();
    addTool(tr("Convert Tabs to Spaces"), TextTools::tabsToSpaces);
    addTool(tr("Convert Spaces to Tabs"), TextTools::spacesToTabs);

    // --- View Menu ---
    QMenu *viewMenu = menuBar->addMenu(tr("&View"));

    QAction *nextTabAct = new QAction(tr("Next &Tab"), m_window);
    nextTabAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Q));
    connect(nextTabAct, &QAction::triggered, m_window, &MicroWriter::nextTab);
    m_window->addAction(nextTabAct);
    viewMenu->addAction(nextTabAct);
    registerAction(nextTabAct);

    viewMenu->addSeparator();
    QAction *newTb = new QAction(tr("Create New Toolbar..."), m_window);
    connect(newTb, &QAction::triggered, m_window, &MicroWriter::createNewToolbar);
    viewMenu->addAction(newTb);

    viewMenu->addSeparator();
    QMenu *layoutMenu = viewMenu->addMenu(tr("&Layout Mode"));
    m_layoutGrp = new QActionGroup(this);
    m_layoutGrp->setExclusive(true);

    auto addLayout = [&](const QString &title, const QKeySequence &seq, LayoutMode mode, bool checked) {
        QAction *act = new QAction(title, m_window);
        act->setCheckable(true);
        if (!seq.isEmpty()) act->setShortcut(seq);
        m_layoutGrp->addAction(act);
        layoutMenu->addAction(act);
        if (checked) act->setChecked(true);
        connect(act, &QAction::triggered, m_window, [this, mode](){ m_window->setLayoutMode(mode); });
        registerAction(act);
        m_layoutActions[mode] = act;
    };

    addLayout(tr("&Normal Layout"), QKeySequence(Qt::CTRL | Qt::Key_1), NormalLayout, true);
    addLayout(tr("&Web Layout"), QKeySequence(Qt::CTRL | Qt::Key_2), WebLayout, false);
    addLayout(tr("&Print Layout"), QKeySequence(Qt::CTRL | Qt::Key_3), PrintLayout, false);
    addLayout(tr("&Code Layout"), QKeySequence(Qt::CTRL | Qt::Key_4), CodeLayout, false);

    viewMenu->addSeparator();
    QAction *darkAct = new QAction(tr("Dark Mode"), m_window);
    darkAct->setCheckable(true);
    connect(darkAct, &QAction::triggered, m_window, &MicroWriter::toggleDarkMode);
    viewMenu->addAction(darkAct);
    m_window->setDarkModeAction(darkAct);

    // --- Help Menu ---
    QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
    QAction *regexHelp = new QAction(tr("Regex Cheat Sheet..."), m_window);
    connect(regexHelp, &QAction::triggered, this, [this](){
        EditorView *ed = m_window->currentEditor();
        if (!ed) return;
        RegexHelpDialog dlg(ed, m_window);
        dlg.exec();
    });
    helpMenu->addAction(regexHelp);

    QAction *htmlHelp = new QAction(tr("HTML Cheat Sheet..."), m_window);
    connect(htmlHelp, &QAction::triggered, this, [this](){
        BaseEditor *base = m_window->currentBaseEditor();
        HtmlHelpDialog dlg(base, m_window);
        dlg.exec();
    });
    helpMenu->addAction(htmlHelp);
}

void ActionManager::setupDefaultToolBar(QToolBar *toolbar) {
    for (QAction *act : m_allMenuActions) {
        if (act->text().contains("Bold") || act->text().contains("Italic") || act->text().contains("Underline") ||
            act->text().contains("Left") || act->text().contains("Center") || act->text().contains("Right") || act->text().contains("Justify")) {
            toolbar->addAction(act);
        }
    }
}
