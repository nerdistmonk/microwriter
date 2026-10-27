#include "dialogs.h"
#include "editorview.h"

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
    resize(250, 140);

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

    QPushButton *okBtn = new QPushButton("Insert", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okBtn);
}

int TableDialog::rows() const { return rowSpin->value(); }
int TableDialog::cols() const { return colSpin->value(); }

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
    resize(320, 160);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("URL (e.g., https://example.com):", this));
    urlInput = new QLineEdit(this);
    urlInput->setPlaceholderText("https://");
    layout->addWidget(urlInput);

    layout->addWidget(new QLabel("Display Text:", this));
    textInput = new QLineEdit(this);
    textInput->setText(defaultText);
    layout->addWidget(textInput);

    QPushButton *okBtn = new QPushButton("Insert Link", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okBtn);
}

QString LinkDialog::url() const { return urlInput->text(); }
QString LinkDialog::linkText() const { return textInput->text(); }

// SymbolDialog
SymbolDialog::SymbolDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Insert Symbol");
    resize(320, 220);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Click a symbol to insert:", this));

    QGridLayout *grid = new QGridLayout();
    QStringList symbols = {
        "©", "®", "™", "°", "§", "¶",
        "±", "×", "÷", "∞", "µ", "¢",
        "£", "¥", "€", "¬", "•", "—",
        "–", "≈", "≠", "≤", "≥", "✔"
    };

    int row = 0, col = 0;
    for (const QString &sym : symbols) {
        QPushButton *btn = new QPushButton(sym, this);
        btn->setFixedSize(40, 40);
        connect(btn, &QPushButton::clicked, this, [=]() {
            selectedSymbol = sym;
            accept();
        });
        grid->addWidget(btn, row, col);
        col++;
        if (col > 5) { col = 0; row++; }
    }
    layout->addLayout(grid);
}

QString SymbolDialog::getSelectedSymbol() const { return selectedSymbol; }

// HeaderFooterDialog
HeaderFooterDialog::HeaderFooterDialog(const QString &header, const QString &footer, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Header & Footer Settings");
    resize(350, 180);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Document Header Text:", this));
    headerInput = new QLineEdit(this);
    headerInput->setText(header);
    layout->addWidget(headerInput);

    layout->addWidget(new QLabel("Document Footer Text:", this));
    footerInput = new QLineEdit(this);
    footerInput->setText(footer);
    layout->addWidget(footerInput);

    QPushButton *okBtn = new QPushButton("Save", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okBtn);
}

QString HeaderFooterDialog::getHeader() const { return headerInput->text(); }
QString HeaderFooterDialog::getFooter() const { return footerInput->text(); }

// FindReplaceDialog
FindReplaceDialog::FindReplaceDialog(QTextEdit *ed, QWidget *parent) : QDialog(parent), editor(ed) {
    setWindowTitle("Find & Replace");
    resize(380, 180);

    QVBoxLayout *layout = new QVBoxLayout(this);
    findInput = new QLineEdit(this);
    findInput->setPlaceholderText("Find what (or regex pattern)...");
    layout->addWidget(findInput);

    replaceInput = new QLineEdit(this);
    replaceInput->setPlaceholderText("Replace with...");
    layout->addWidget(replaceInput);

    regexCheckBox = new QCheckBox("Use Regular Expression (Regex)", this);
    layout->addWidget(regexCheckBox);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *findBtn = new QPushButton("Find Next", this);
    QPushButton *replaceBtn = new QPushButton("Replace", this);
    QPushButton *replaceAllBtn = new QPushButton("Replace All", this);

    btnLayout->addWidget(findBtn);
    btnLayout->addWidget(replaceBtn);
    btnLayout->addWidget(replaceAllBtn);
    layout->addLayout(btnLayout);

    connect(findBtn, &QPushButton::clicked, this, &FindReplaceDialog::findNext);
    connect(replaceBtn, &QPushButton::clicked, this, &FindReplaceDialog::replaceCurrent);
    connect(replaceAllBtn, &QPushButton::clicked, this, &FindReplaceDialog::replaceAll);
}

void FindReplaceDialog::setPattern(const QString &findText, const QString &replaceText, bool useRegex) {
    findInput->setText(findText);
    replaceInput->setText(replaceText);
    if (regexCheckBox) regexCheckBox->setChecked(useRegex);
}

void FindReplaceDialog::findNext() {
    QString query = findInput->text();
    if (query.isEmpty() || !editor) return;

    if (regexCheckBox->isChecked()) {
        QRegularExpression re(query);
        if (!re.isValid()) return;
        QTextDocument::FindFlags flags;
        bool found = editor->find(re, flags);
        if (!found) {
            QTextCursor cursor = editor->textCursor();
            cursor.movePosition(QTextCursor::Start);
            editor->setTextCursor(cursor);
            editor->find(re, flags);
        }
    } else {
        bool found = editor->find(query);
        if (!found) {
            QTextCursor cursor = editor->textCursor();
            cursor.movePosition(QTextCursor::Start);
            editor->setTextCursor(cursor);
            editor->find(query);
        }
    }
}

void FindReplaceDialog::replaceCurrent() {
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        if (regexCheckBox->isChecked()) {
            QString query = findInput->text();
            QRegularExpression re(query);
            if (re.isValid()) {
                QString selected = cursor.selectedText();
                QString replaced = selected.replace(re, replaceInput->text());
                cursor.insertText(replaced);
            }
        } else if (cursor.selectedText() == findInput->text()) {
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

    if (regexCheckBox->isChecked()) {
        QRegularExpression re(query);
        if (!re.isValid()) {
            cursor.endEditBlock();
            return;
        }
        while (editor->find(re)) {
            QTextCursor c = editor->textCursor();
            QString selected = c.selectedText();
            QString replaced = selected.replace(re, replacement);
            c.insertText(replaced);
        }
    } else {
        while (editor->find(query)) {
            QTextCursor c = editor->textCursor();
            c.insertText(replacement);
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
        {"Cut everything before character (e.g. up to 'Skater')", ".*(Skater)", "\\1"},
        {"Remove file extension (e.g. .z64, .txt)", "\\.[^.]+$", ""},
        {"Find all numbers", "\\d+", ""},
        {"Remove trailing whitespace", "\\s+$", ""}
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
    connect(tableWidget, &QTableWidget::doubleClicked, this, &RegexHelpDialog::applySelected);
    layout->addWidget(useBtn);
}

void RegexHelpDialog::applySelected() {
    int row = tableWidget->currentRow();
    if (row < 0) return;
    QString findStr = tableWidget->item(row, 1)->text();
    QString repStr = tableWidget->item(row, 2)->text();

    accept();

    FindReplaceDialog *dlg = new FindReplaceDialog(editor, parentWidget());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setPattern(findStr, repStr, true);
    dlg->show();
}

// ToolbarCustomizeDialog
ToolbarCustomizeDialog::ToolbarCustomizeDialog(QToolBar *targetToolbar, const QList<QAction*> &allActions, QWidget *parent)
    : QDialog(parent), toolbar(targetToolbar), availableActions(allActions)
{
    setWindowTitle(QString("Customize Toolbar - %1").arg(targetToolbar->windowTitle()));
    resize(500, 350);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    QVBoxLayout *availLayout = new QVBoxLayout();
    availLayout->addWidget(new QLabel("Available Actions:", this));
    availableList = new QListWidget(this);
    availLayout->addWidget(availableList);

    QVBoxLayout *activeLayout = new QVBoxLayout();
    activeLayout->addWidget(new QLabel("Active Actions in Toolbar:", this));
    activeList = new QListWidget(this);
    activeLayout->addWidget(activeList);

    QVBoxLayout *btnLayout = new QVBoxLayout();
    QPushButton *addBtn = new QPushButton("Add ->", this);
    QPushButton *removeBtn = new QPushButton("<- Remove", this);
    btnLayout->addStretch();
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();

    mainLayout->addLayout(availLayout);
    mainLayout->addLayout(btnLayout);
    mainLayout->addLayout(activeLayout);

    populateLists();

    connect(addBtn, &QPushButton::clicked, this, &ToolbarCustomizeDialog::addActionToToolbar);
    connect(removeBtn, &QPushButton::clicked, this, &ToolbarCustomizeDialog::removeActionFromToolbar);
}

void ToolbarCustomizeDialog::populateLists() {
    availableList->clear();
    activeList->clear();

    QList<QAction*> currentToolbarActions = toolbar->actions();

    for (QAction *act : availableActions) {
        if (!act || act->text().isEmpty()) continue;

        QString cleanText = act->text();
        cleanText.remove('&');

        QListWidgetItem *item = new QListWidgetItem(cleanText);
        item->setData(Qt::UserRole, QVariant::fromValue(act));

        if (currentToolbarActions.contains(act)) {
            activeList->addItem(item);
        } else {
            availableList->addItem(item);
        }
    }
}

void ToolbarCustomizeDialog::addActionToToolbar() {
    QListWidgetItem *item = availableList->currentItem();
    if (!item) return;

    QAction *act = item->data(Qt::UserRole).value<QAction*>();
    if (act) {
        toolbar->addAction(act);
        populateLists();
    }
}

void ToolbarCustomizeDialog::removeActionFromToolbar() {
    QListWidgetItem *item = activeList->currentItem();
    if (!item) return;

    QAction *act = item->data(Qt::UserRole).value<QAction*>();
    if (act) {
        toolbar->removeAction(act);
        populateLists();
    }
}
