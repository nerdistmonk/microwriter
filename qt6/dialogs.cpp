#include "dialogs.h"
#include "editorview.h"
#include "codeeditorview.h"
#include "baseeditor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QListWidget>
#include <QDateTime>
#include <QRegularExpression>
#include <QToolBar>
#include <QAction>

// TableDialog
TableDialog::TableDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Insert Table");
    resize(280, 180);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Rows:", this));
    rowSpin = new QSpinBox(this);
    rowSpin->setRange(1, 50);
    rowSpin->setValue(3);
    layout->addWidget(rowSpin);

    layout->addWidget(new QLabel("Columns:", this));
    colSpin = new QSpinBox(this);
    colSpin->setRange(1, 20);
    colSpin->setValue(3);
    layout->addWidget(colSpin);

    layout->addWidget(new QLabel("Table Alignment:", this));
    alignCombo = new QComboBox(this);
    alignCombo->addItem("Left", static_cast<int>(Qt::AlignLeft));
    alignCombo->addItem("Center", static_cast<int>(Qt::AlignHCenter));
    alignCombo->addItem("Right", static_cast<int>(Qt::AlignRight));
    alignCombo->setCurrentIndex(0); // Default to Left so tables hug the left margin like Markdown
    layout->addWidget(alignCombo);

    QPushButton *okBtn = new QPushButton("Insert", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okBtn);
}

int TableDialog::rows() const { return rowSpin->value(); }
int TableDialog::cols() const { return colSpin->value(); }
Qt::Alignment TableDialog::alignment() const {
    return static_cast<Qt::Alignment>(alignCombo->currentData().toInt());
}

// DateTimeDialog
DateTimeDialog::DateTimeDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Insert Date & Time");
    resize(300, 130);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Select Format:", this));

    formatCombo = new QComboBox(this);
    QDateTime now = QDateTime::currentDateTime();

    formatCombo->addItem(now.toString("yyyy-MM-dd hh:mm:ss"), "yyyy-MM-dd hh:mm:ss");
    formatCombo->addItem(now.toString("MM/dd/yyyy"), "MM/dd/yyyy");
    formatCombo->addItem(now.toString("dd.MM.yyyy"), "dd.MM.yyyy");
    formatCombo->addItem(now.toString("MMMM d, yyyy"), "MMMM d, yyyy");
    formatCombo->addItem(now.toString("hh:mm AP"), "hh:mm AP");
    formatCombo->addItem(now.toString("dddd, MMMM d, yyyy"), "dddd, MMMM d, yyyy");

    layout->addWidget(formatCombo);

    QPushButton *okBtn = new QPushButton("Insert", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okBtn);
}

QString DateTimeDialog::selectedFormatString() const {
    return formatCombo->currentData().toString();
}

// LinkDialog
LinkDialog::LinkDialog(const QString &defaultText, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Insert Link");
    resize(320, 140);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *textLayout = new QHBoxLayout();
    textLayout->addWidget(new QLabel("Display Text:", this));
    textInput = new QLineEdit(defaultText, this);
    textLayout->addWidget(textInput);
    layout->addLayout(textLayout);

    QHBoxLayout *urlLayout = new QHBoxLayout();
    urlLayout->addWidget(new QLabel("URL:", this));
    urlInput = new QLineEdit("https://", this);
    urlLayout->addWidget(urlInput);
    layout->addLayout(urlLayout);

    QPushButton *okBtn = new QPushButton("Insert Link", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okBtn);
}

QString LinkDialog::url() const { return urlInput->text(); }
QString LinkDialog::linkText() const { return textInput->text(); }

// SymbolDialog
SymbolDialog::SymbolDialog(QWidget *parent) : QDialog(parent), selectedSymbol("") {
    setWindowTitle("Insert Symbol");
    resize(320, 240);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QGridLayout *grid = new QGridLayout();

    QStringList symbols = {
        "©", "®", "™", "§", "¶", "†", "‡", "•", "–", "—",
        "€", "£", "¥", "¢", "±", "×", "÷", "≠", "≈", "≤",
        "≥", "∞", "°", "µ", "π", "Ω", "α", "β", "√", "∆"
    };

    int row = 0;
    int col = 0;
    for (const QString &sym : symbols) {
        QPushButton *btn = new QPushButton(sym, this);
        btn->setFixedSize(36, 36);
        connect(btn, &QPushButton::clicked, this, [this, sym](){
            selectedSymbol = sym;
            accept();
        });
        grid->addWidget(btn, row, col);
        col++;
        if (col >= 6) {
            col = 0;
            row++;
        }
    }

    mainLayout->addLayout(grid);
}

QString SymbolDialog::getSelectedSymbol() const { return selectedSymbol; }

// HeaderFooterDialog
HeaderFooterDialog::HeaderFooterDialog(const QString &header, const QString &footer, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Page Header & Footer");
    resize(350, 160);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->addWidget(new QLabel("Header Text:", this));
    headerInput = new QLineEdit(header, this);
    hLayout->addWidget(headerInput);
    layout->addLayout(hLayout);

    QHBoxLayout *fLayout = new QHBoxLayout();
    fLayout->addWidget(new QLabel("Footer Text:", this));
    footerInput = new QLineEdit(footer, this);
    fLayout->addWidget(footerInput);
    layout->addLayout(fLayout);

    QPushButton *okBtn = new QPushButton("Apply", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okBtn);
}

QString HeaderFooterDialog::getHeader() const { return headerInput->text(); }
QString HeaderFooterDialog::getFooter() const { return footerInput->text(); }

// FindReplaceDialog
FindReplaceDialog::FindReplaceDialog(BaseEditor *ed, QWidget *parent) : QDialog(parent), editor(ed) {
    setWindowTitle("Find and Replace");
    resize(380, 180);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *findLayout = new QHBoxLayout();
    findLayout->addWidget(new QLabel("Find:", this));
    findInput = new QLineEdit(this);
    findLayout->addWidget(findInput);
    mainLayout->addLayout(findLayout);

    QHBoxLayout *replaceLayout = new QHBoxLayout();
    replaceLayout->addWidget(new QLabel("Replace:", this));
    replaceInput = new QLineEdit(this);
    replaceLayout->addWidget(replaceInput);
    mainLayout->addLayout(replaceLayout);

    regexCheckBox = new QCheckBox("Use Regular Expressions", this);
    mainLayout->addWidget(regexCheckBox);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *findBtn = new QPushButton("Find Next", this);
    QPushButton *replaceBtn = new QPushButton("Replace", this);
    QPushButton *replaceAllBtn = new QPushButton("Replace All", this);

    btnLayout->addWidget(findBtn);
    btnLayout->addWidget(replaceBtn);
    btnLayout->addWidget(replaceAllBtn);
    mainLayout->addLayout(btnLayout);

    connect(findBtn, &QPushButton::clicked, this, &FindReplaceDialog::findNext);
    connect(replaceBtn, &QPushButton::clicked, this, &FindReplaceDialog::replaceCurrent);
    connect(replaceAllBtn, &QPushButton::clicked, this, &FindReplaceDialog::replaceAll);
}

void FindReplaceDialog::setPattern(const QString &findText, const QString &replaceText, bool useRegex) {
    findInput->setText(findText);
    replaceInput->setText(replaceText);
    regexCheckBox->setChecked(useRegex);
}

void FindReplaceDialog::findNext() {
    QString query = findInput->text();
    if (query.isEmpty() || !editor) return;

    if (regexCheckBox->isChecked()) {
        QRegularExpression re(query);
        if (!re.isValid()) return;
        if (!editor->findRegExp(re)) {
            editor->moveCursor(QTextCursor::Start);
            editor->findRegExp(re);
        }
    } else {
        if (!editor->findText(query)) {
            editor->moveCursor(QTextCursor::Start);
            editor->findText(query);
        }
    }
}

void FindReplaceDialog::replaceCurrent() {
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        if (regexCheckBox->isChecked()) {
            QRegularExpression re(findInput->text());
            QString selected = cursor.selectedText();
            selected.replace(re, replaceInput->text());
            cursor.insertText(selected);
        } else {
            cursor.insertText(replaceInput->text());
        }
    }
    findNext();
}

void FindReplaceDialog::replaceAll() {
    QString query = findInput->text();
    QString replacement = replaceInput->text();
    if (query.isEmpty() || !editor) return;

    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);
    editor->setTextCursor(cursor);

    int count = 0;
    const int maxReplacements = 100000;

    if (regexCheckBox->isChecked()) {
        QRegularExpression re(query);
        if (!re.isValid()) {
            cursor.endEditBlock();
            return;
        }

        while (editor->findRegExp(re)) {
            QTextCursor c = editor->textCursor();
            QString selected = c.selectedText();

            if (selected.isEmpty()) {
                if (!c.movePosition(QTextCursor::NextCharacter)) {
                    break;
                }
                editor->setTextCursor(c);
                continue;
            }

            QString replaced = selected;
            replaced.replace(re, replacement);
            c.insertText(replaced);

            if (++count >= maxReplacements) break;
        }
    } else {
        while (editor->findText(query)) {
            QTextCursor c = editor->textCursor();
            if (c.selectedText().isEmpty()) {
                if (!c.movePosition(QTextCursor::NextCharacter)) {
                    break;
                }
                editor->setTextCursor(c);
                continue;
            }
            c.insertText(replacement);

            if (++count >= maxReplacements) break;
        }
    }
    cursor.endEditBlock();
}

// RegexHelpDialog
RegexHelpDialog::RegexHelpDialog(EditorView *ed, QWidget *parent) : QDialog(parent), editor(ed) {
    setWindowTitle("Regex Cheat Sheet & Quick Apply");
    resize(650, 400);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Double-click any example (or select and click 'Use Selected Pattern') to load it into Find & Replace:", this));

    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(3);
    tableWidget->setHorizontalHeaderLabels({"Description", "Find Pattern", "Replace Pattern"});
    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    struct PatternInfo { QString desc; QString find; QString rep; };
    QList<PatternInfo> examples = {
        {"Cut everything after character (e.g. 'r' in Skater)", "(r).*", "\\1"},
        {"Cut everything before character (e.g. up to 'Ska')", ".*(Ska)", "\\1"},
        {"Keep characters inside quotes", "\"([^\"]*)\"", "\\1"},
        {"Remove all digits / numbers", "\\d+", ""},
        {"Remove leading whitespace per line", "^[ \\t]+", ""},
        {"Remove trailing whitespace per line", "[ \\t]+$", ""},
        {"Replace multiple spaces with single space", "[ \\t]+", " "},
        {"Extract email addresses", "([a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+)", "\\1"},
        {"Convert Markdown headers (# ) to upper", "^#+\\s*(.*)$", "\\1"}
    };

    tableWidget->setRowCount(examples.size());
    for (int i = 0; i < examples.size(); ++i) {
        tableWidget->setItem(i, 0, new QTableWidgetItem(examples[i].desc));
        tableWidget->setItem(i, 1, new QTableWidgetItem(examples[i].find));
        tableWidget->setItem(i, 2, new QTableWidgetItem(examples[i].rep));
    }
    layout->addWidget(tableWidget);

    QPushButton *useBtn = new QPushButton("Use Selected Pattern", this);
    connect(useBtn, &QPushButton::clicked, this, &RegexHelpDialog::applySelected);
    connect(tableWidget, &QTableWidget::cellDoubleClicked, this, [this](int, int){ applySelected(); });
    layout->addWidget(useBtn);
}

void RegexHelpDialog::applySelected() {
    int row = tableWidget->currentRow();
    if (row < 0) return;

    QString f = tableWidget->item(row, 1)->text();
    QString r = tableWidget->item(row, 2)->text();

    accept();

    if (editor) {
        FindReplaceDialog *dlg = new FindReplaceDialog(editor, parentWidget());
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setPattern(f, r, true);
        dlg->show();
    }
}

// HtmlHelpDialog
HtmlHelpDialog::HtmlHelpDialog(BaseEditor *ed, QWidget *parent) : QDialog(parent), editor(ed) {
    setWindowTitle("HTML Quick Reference");
    resize(500, 360);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Double-click a tag to insert it into the current document:", this));

    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(2);
    tableWidget->setHorizontalHeaderLabels({"Description", "HTML Tag"});
    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    struct HtmlSnippet { QString desc; QString tag; };
    QList<HtmlSnippet> snippets = {
        {"Paragraph", "<p></p>"},
        {"Heading 1", "<h1></h1>"},
        {"Heading 2", "<h2></h2>"},
        {"Bold text", "<b></b>"},
        {"Italic text", "<i></i>"},
        {"Anchor / Link", "<a href=\"\"></a>"},
        {"Image element", "<img src=\"\" alt=\"\" />"},
        {"Unordered List", "<ul>\n  <li></li>\n</ul>"},
        {"Ordered List", "<ol>\n  <li></li>\n</ol>"},
        {"Blockquote", "<blockquote></blockquote>"},
        {"Inline Code", "<code></code>"},
        {"Code Block", "<pre><code></code></pre>"},
        {"Horizontal Rule", "<hr />"}
    };

    tableWidget->setRowCount(snippets.size());
    for (int i = 0; i < snippets.size(); ++i) {
        tableWidget->setItem(i, 0, new QTableWidgetItem(snippets[i].desc));
        tableWidget->setItem(i, 1, new QTableWidgetItem(snippets[i].tag));
    }
    layout->addWidget(tableWidget);

    QPushButton *insBtn = new QPushButton("Insert Tag", this);
    connect(insBtn, &QPushButton::clicked, this, &HtmlHelpDialog::applySelected);
    connect(tableWidget, &QTableWidget::cellDoubleClicked, this, [this](int, int){ applySelected(); });
    layout->addWidget(insBtn);
}

void HtmlHelpDialog::applySelected() {
    int row = tableWidget->currentRow();
    if (row < 0 || !editor) return;

    QString tag = tableWidget->item(row, 1)->text();
    QTextCursor c = editor->textCursor();
    c.insertText(tag);
    editor->setTextCursor(c);
    accept();
}

// SedHelpDialog
SedHelpDialog::SedHelpDialog(BaseEditor *ed, QWidget *parent) : QDialog(parent), editor(ed) {
    setWindowTitle("Sed & Unix Utilities Cheat Sheet");
    resize(650, 400);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Double-click a command or select and click 'Insert Command' to insert it into the editor:", this));

    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(2);
    tableWidget->setHorizontalHeaderLabels({"Description", "Command"});
    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    struct SedSnippet { QString desc; QString cmd; };
    QList<SedSnippet> snippets = {
        {"Replace first occurrence per line", "sed 's/foo/bar/' file.txt"},
        {"Global search and replace", "sed 's/foo/bar/g' file.txt"},
        {"In-place file replace", "sed -i 's/foo/bar/g' file.txt"},
        {"Delete empty lines", "sed '/^$/d' file.txt"},
        {"Delete lines matching pattern", "sed '/pattern/d' file.txt"},
        {"Print specific line numbers (e.g. 5-10)", "sed -n '5,10p' file.txt"},
        {"Print specific column (col 1)", "awk '{print $1}' file.txt"},
        {"Sum values in column 1", "awk '{s+=$1} END {print s}' file.txt"},
        {"Extract delimiter separated fields (CSV)", "cut -d',' -f1,3 file.csv"},
        {"Recursive case-insensitive with line numbers", "grep -rnI 'search_term' ."},
        {"Translate uppercase to lowercase", "cat file.txt | tr '[:upper:]' '[:lower:]'"},
        {"Parallel command execution (4 jobs)", "cat files.txt | xargs -n 1 -P 4 command"}
    };

    tableWidget->setRowCount(snippets.size());
    for (int i = 0; i < snippets.size(); ++i) {
        tableWidget->setItem(i, 0, new QTableWidgetItem(snippets[i].desc));
        tableWidget->setItem(i, 1, new QTableWidgetItem(snippets[i].cmd));
    }
    layout->addWidget(tableWidget);

    QPushButton *insBtn = new QPushButton("Insert Command", this);
    connect(insBtn, &QPushButton::clicked, this, &SedHelpDialog::applySelected);
    connect(tableWidget, &QTableWidget::cellDoubleClicked, this, [this](int, int){ applySelected(); });
    layout->addWidget(insBtn);
}

void SedHelpDialog::applySelected() {
    int row = tableWidget->currentRow();
    if (row < 0 || !editor) return;

    QString cmd = tableWidget->item(row, 1)->text();
    QTextCursor c = editor->textCursor();
    c.insertText(cmd);
    editor->setTextCursor(c);
    accept();
}

// BashHelpDialog
BashHelpDialog::BashHelpDialog(BaseEditor *ed, QWidget *parent) : QDialog(parent), editor(ed) {
    setWindowTitle("Useful Bash Scripts");
    resize(650, 400);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Double-click a script or select and click 'Insert Script' to insert it into the editor:", this));

    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount(2);
    tableWidget->setHorizontalHeaderLabels({"Title", "Preview"});
    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    struct ScriptSnippet { QString title; QString code; };
    QList<ScriptSnippet> scripts = {
        {
            "Strict Mode Boilerplate (set -euo pipefail)",
            "#!/usr/bin/env bash\nset -euo pipefail\nIFS=$'\\n\\t'\n\n# Main entry point\nmain() {\n    echo \"Starting script...\"\n}\nmain \"$@\"\n"
        },
        {
            "CLI Option & Argument Parser (getopts)",
            "while getopts \":f:h\" opt; do\n    case ${opt} in\n        f ) FILE=\"$OPTARG\" ;;\n        h ) echo \"Usage: $0 [-f file]\"; exit 0 ;;\n        \\? ) echo \"Invalid Option: -$OPTARG\" 1>&2; exit 1 ;;\n    esac\ndone\n"
        },
        {
            "Traverse Directory & Loop Over Files Safe",
            "while IFS= read -r -d '' file; do\n    echo \"Processing: $file\"\ndone < <(find . -type f -name \"*.cpp\" -print0)\n"
        },
        {
            "Temporary Directory with Auto-Cleanup Trap",
            "TMP_DIR=$(mktemp -d)\ntrap 'rm -rf \"$TMP_DIR\"' EXIT INT TERM\necho \"Using temporary directory: $TMP_DIR\"\n"
        },
        {
            "Check Command Existence / Prerequisite Guard",
            "command -v jq >/dev/null 2>&1 || { echo \"Error: 'jq' is required but not installed.\" >&2; exit 1; }\n"
        },
        {
            "Prompt User for Confirmation (y/N)",
            "read -r -p \"Are you sure you want to proceed? [y/N] \" response\ncase \"$response\" in\n    [yY][eE][sS]|[yY]) echo \"Confirmed.\" ;;\n    *) echo \"Aborted.\"; exit 1 ;;\nesac\n"
        }
    };

    tableWidget->setRowCount(scripts.size());
    for (int i = 0; i < scripts.size(); ++i) {
        tableWidget->setItem(i, 0, new QTableWidgetItem(scripts[i].title));
        QString firstLine = scripts[i].code.split('\n').first();
        tableWidget->setItem(i, 1, new QTableWidgetItem(firstLine + " ..."));
        tableWidget->item(i, 0)->setData(Qt::UserRole, scripts[i].code);
    }
    layout->addWidget(tableWidget);

    QPushButton *insBtn = new QPushButton("Insert Script", this);
    connect(insBtn, &QPushButton::clicked, this, &BashHelpDialog::applySelected);
    connect(tableWidget, &QTableWidget::cellDoubleClicked, this, [this](int, int){ applySelected(); });
    layout->addWidget(insBtn);
}

void BashHelpDialog::applySelected() {
    int row = tableWidget->currentRow();
    if (row < 0 || !editor) return;

    QString code = tableWidget->item(row, 0)->data(Qt::UserRole).toString();
    QTextCursor c = editor->textCursor();
    c.insertText(code);
    editor->setTextCursor(c);
    accept();
}

// ToolbarCustomizeDialog
ToolbarCustomizeDialog::ToolbarCustomizeDialog(QToolBar *targetToolbar, const QList<QAction*> &allActions, QWidget *parent)
    : QDialog(parent), toolbar(targetToolbar), availableActions(allActions)
{
    setWindowTitle("Customize Toolbar");
    resize(500, 350);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *listLayout = new QHBoxLayout();

    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(new QLabel("Available Actions:", this));
    availableList = new QListWidget(this);
    leftLayout->addWidget(availableList);

    QVBoxLayout *midLayout = new QVBoxLayout();
    QPushButton *addBtn = new QPushButton("->", this);
    QPushButton *removeBtn = new QPushButton("<-", this);
    midLayout->addStretch();
    midLayout->addWidget(addBtn);
    midLayout->addWidget(removeBtn);
    midLayout->addStretch();

    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(new QLabel("Current Toolbar Actions:", this));
    activeList = new QListWidget(this);
    rightLayout->addWidget(activeList);

    listLayout->addLayout(leftLayout);
    listLayout->addLayout(midLayout);
    listLayout->addLayout(rightLayout);
    mainLayout->addLayout(listLayout);

    QPushButton *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeBtn);

    connect(addBtn, &QPushButton::clicked, this, &ToolbarCustomizeDialog::addActionToToolbar);
    connect(removeBtn, &QPushButton::clicked, this, &ToolbarCustomizeDialog::removeActionFromToolbar);

    populateLists();
}

void ToolbarCustomizeDialog::populateLists() {
    availableList->clear();
    activeList->clear();

    QList<QAction*> currentActions = toolbar->actions();

    for (QAction *act : availableActions) {
        if (!currentActions.contains(act) && !act->text().isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem(act->text(), availableList);
            item->setData(Qt::UserRole, QVariant::fromValue((void*)act));
        }
    }

    for (QAction *act : currentActions) {
        if (!act->text().isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem(act->text(), activeList);
            item->setData(Qt::UserRole, QVariant::fromValue((void*)act));
        }
    }
}

void ToolbarCustomizeDialog::addActionToToolbar() {
    QListWidgetItem *item = availableList->currentItem();
    if (!item) return;

    QAction *act = static_cast<QAction*>(item->data(Qt::UserRole).value<void*>());
    if (act) {
        toolbar->addAction(act);
        populateLists();
    }
}

void ToolbarCustomizeDialog::removeActionFromToolbar() {
    QListWidgetItem *item = activeList->currentItem();
    if (!item) return;

    QAction *act = static_cast<QAction*>(item->data(Qt::UserRole).value<void*>());
    if (act) {
        toolbar->removeAction(act);
        populateLists();
    }
}
