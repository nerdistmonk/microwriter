#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QTextEdit>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QTextStream>
#include <QKeySequence>
#include <QFontComboBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QLabel>
#include <QRegularExpression>
#include <QColorDialog>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextDocumentWriter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QImage>
#include <QTextListFormat>
#include <QPainter>
#include <QProcess>
#include <QMessageBox>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QTextDocumentFragment>
#include <QTextTableFormat>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QActionGroup>
#include <QScrollBar>
#include <QSettings>
#include <QStringConverter>
#include <QInputDialog>
#include <QCloseEvent>
#include <QTextFrame>
#include <QTextBlockFormat>
#include <algorithm>
#include <functional>

// Forward declaration
class EditorView;

// Layout Mode Enum
enum LayoutMode {
    NormalLayout,
    WebLayout,
    PrintLayout
};

// Line Ending Enum
enum EolFormat {
    EolLf,   // Unix (\n)
    EolCrlf, // Windows (\r\n)
    EolCr    // Mac (\r)
};

// Subclassed QTextEdit with page break rendering, header/footer storage, and encoding/EOL metadata
class EditorView : public QTextEdit {
    Q_OBJECT
public:
    EditorView(QWidget *parent = nullptr) 
        : QTextEdit(parent), filePath(""), isPrintLayout(false), encodingName("UTF-8"), eolFormat(EolLf),
          _headerText(""), _footerText("") {
        
        // Default document frame margins (in pixels)
        QTextFrameFormat fmt = document()->rootFrame()->frameFormat();
        fmt.setLeftMargin(20);
        fmt.setRightMargin(20);
        document()->rootFrame()->setFrameFormat(fmt);
    }
    
    QString filePath;
    bool isPrintLayout;
    QString encodingName;
    EolFormat eolFormat;

    QString headerText() const { return _headerText; }
    void setHeaderText(const QString &text) { _headerText = text; }

    QString footerText() const { return _footerText; }
    void setFooterText(const QString &text) { _footerText = text; }

    void setLeftMarginPixels(qreal pixels) {
        QTextFrameFormat fmt = document()->rootFrame()->frameFormat();
        fmt.setLeftMargin(pixels);
        document()->rootFrame()->setFrameFormat(fmt);
    }

    void setRightMarginPixels(qreal pixels) {
        QTextFrameFormat fmt = document()->rootFrame()->frameFormat();
        fmt.setRightMargin(pixels);
        document()->rootFrame()->setFrameFormat(fmt);
    }

    qreal leftMarginPixels() const {
        return document()->rootFrame()->frameFormat().leftMargin();
    }

    qreal rightMarginPixels() const {
        return document()->rootFrame()->frameFormat().rightMargin();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QTextEdit::paintEvent(event);

        if (isPrintLayout) {
            QPainter painter(viewport());
            painter.setPen(QPen(QColor(200, 50, 50), 2, Qt::DashLine));
            painter.setFont(QFont("Arial", 9, QFont::Bold));

            int pageHeight = 1056; // 11 inches at 96 DPI
            int scrollOffset = verticalScrollBar()->value();
            int totalDocHeight = document()->size().height();

            for (int y = pageHeight; y < totalDocHeight; y += pageHeight) {
                int viewY = y - scrollOffset;
                if (viewY >= 0 && viewY <= viewport()->height()) {
                    painter.drawLine(0, viewY, viewport()->width(), viewY);
                    painter.setPen(QColor(150, 30, 30));
                    painter.drawText(20, viewY - 6, QString("=== Page Break (Page %1) ===").arg((y / pageHeight) + 1));
                    painter.setPen(QPen(QColor(200, 50, 50), 2, Qt::DashLine));
                }
            }
        }
    }

    void contextMenuEvent(QContextMenuEvent *event) override {
        QMenu *menu = createStandardContextMenu();
        
        QTextCursor cursor = cursorForPosition(event->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        QString word = cursor.selectedText();

        if (!word.trimmed().isEmpty()) {
            QStringList suggestions = getAspellSuggestions(word);
            if (!suggestions.isEmpty()) {
                menu->addSeparator();
                QMenu *spellMenu = menu->addMenu("Spelling Suggestions");
                for (const QString &suggestion : suggestions) {
                    QAction *action = spellMenu->addAction(suggestion);
                    connect(action, &QAction::triggered, this, [=]() {
                        QTextCursor c = cursorForPosition(event->pos());
                        c.select(QTextCursor::WordUnderCursor);
                        c.insertText(suggestion);
                    });
                }
            }
        }
        
        menu->exec(event->globalPos());
        delete menu;
    }

private:
    QString _headerText;
    QString _footerText;

    QStringList getAspellSuggestions(const QString &word) {
        QProcess process;
        process.start("aspell", QStringList() << "-a");
        if (!process.waitForStarted()) return QStringList();
        
        process.write(word.toUtf8() + "\n");
        process.closeWriteChannel();
        process.waitForFinished();
        
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList suggestions;
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            if (line.startsWith('&')) {
                int colonIndex = line.indexOf(':');
                if (colonIndex != -1) {
                    QString sugPart = line.mid(colonIndex + 1).trimmed();
                    suggestions = sugPart.split(", ");
                }
            }
        }
        return suggestions;
    }
};

// Interactive Inch-based Margin Ruler
class Ruler : public QWidget {
    Q_OBJECT
public:
    Ruler(QWidget *parent = nullptr) : QWidget(parent), editor(nullptr), draggingLeft(false), draggingRight(false) {
        setFixedHeight(28);
        setMouseTracking(true);
        leftMarginPos = 30;
        rightMarginPos = 700;
    }

    void setEditor(EditorView *ed) { 
        editor = ed; 
        syncWithEditor();
        update(); 
    }

    void syncWithEditor() {
        if (!editor) return;
        leftMarginPos = qRound(editor->leftMarginPixels());
        rightMarginPos = width() - qRound(editor->rightMarginPixels());
        leftMarginPos = qBound(5, leftMarginPos, width() - 50);
        rightMarginPos = qBound(leftMarginPos + 40, rightMarginPos, width() - 5);
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        syncWithEditor();
    }

    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(240, 240, 240));
        
        painter.setPen(QColor(180, 180, 180));
        painter.drawLine(0, height() - 1, width(), height() - 1);

        int dpi = 96;
        painter.setPen(Qt::darkGray);

        for (int i = 0; i < width(); i += dpi) {
            int inch = i / dpi;
            painter.drawLine(i, height(), i, height() - 10);
            if (inch > 0) {
                painter.drawText(i - 15, 2, 30, 12, Qt::AlignCenter, QString::number(inch) + "\"");
            }
            
            for (int q = 1; q < 4; ++q) {
                int qPos = i + (q * dpi / 4);
                int h = (q == 2) ? 8 : 4;
                painter.drawLine(qPos, height(), qPos, height() - h);
            }
        }

        painter.setBrush(Qt::darkBlue);
        painter.setPen(Qt::NoPen);
        
        // Left Margin Handle
        QPolygon leftTri;
        leftTri << QPoint(leftMarginPos, height()) 
                << QPoint(leftMarginPos - 6, height() - 10) 
                << QPoint(leftMarginPos + 6, height() - 10);
        painter.drawPolygon(leftTri);

        // Right Margin Handle
        QPolygon rightTri;
        rightTri << QPoint(rightMarginPos, height()) 
                 << QPoint(rightMarginPos - 6, height() - 10) 
                 << QPoint(rightMarginPos + 6, height() - 10);
        painter.drawPolygon(rightTri);
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (!editor) return;
        if (qAbs(event->pos().x() - leftMarginPos) < 10) {
            draggingLeft = true;
        } else if (qAbs(event->pos().x() - rightMarginPos) < 10) {
            draggingRight = true;
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        int x = event->pos().x();
        if (draggingLeft) {
            leftMarginPos = qBound(5, x, rightMarginPos - 40);
            if (editor) {
                editor->setLeftMarginPixels(leftMarginPos);
            }
            update();
        } else if (draggingRight) {
            rightMarginPos = qBound(leftMarginPos + 40, x, width() - 5);
            if (editor) {
                editor->setRightMarginPixels(width() - rightMarginPos);
            }
            update();
        } else {
            if (qAbs(x - leftMarginPos) < 10 || qAbs(x - rightMarginPos) < 10) {
                setCursor(Qt::SplitHCursor);
            } else {
                setCursor(Qt::ArrowCursor);
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent *) override {
        draggingLeft = false;
        draggingRight = false;
    }

private:
    EditorView *editor;
    int leftMarginPos;
    int rightMarginPos;
    bool draggingLeft;
    bool draggingRight;
};

class TableDialog : public QDialog {
    Q_OBJECT
public:
    TableDialog(QWidget *parent = nullptr) : QDialog(parent) {
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

    int rows() const { return rowSpin->value(); }
    int cols() const { return colSpin->value(); }

private:
    QSpinBox *rowSpin;
    QSpinBox *colSpin;
};

class DateTimeDialog : public QDialog {
    Q_OBJECT
public:
    DateTimeDialog(QWidget *parent = nullptr) : QDialog(parent) {
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

    QString selectedFormatString() const {
        return formatCombo->currentData().toString();
    }

private:
    QComboBox *formatCombo;
};

class LinkDialog : public QDialog {
    Q_OBJECT
public:
    LinkDialog(const QString &defaultText = "", QWidget *parent = nullptr) : QDialog(parent) {
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

    QString url() const { return urlInput->text(); }
    QString linkText() const { return textInput->text(); }

private:
    QLineEdit *urlInput;
    QLineEdit *textInput;
};

class SymbolDialog : public QDialog {
    Q_OBJECT
public:
    SymbolDialog(QWidget *parent = nullptr) : QDialog(parent) {
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

    QString getSelectedSymbol() const { return selectedSymbol; }

private:
    QString selectedSymbol;
};

class HeaderFooterDialog : public QDialog {
    Q_OBJECT
public:
    HeaderFooterDialog(const QString &header, const QString &footer, QWidget *parent = nullptr) : QDialog(parent) {
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

    QString getHeader() const { return headerInput->text(); }
    QString getFooter() const { return footerInput->text(); }

private:
    QLineEdit *headerInput;
    QLineEdit *footerInput;
};

class FindReplaceDialog : public QDialog {
    Q_OBJECT
public:
    FindReplaceDialog(QTextEdit *ed, QWidget *parent = nullptr) : QDialog(parent), editor(ed) {
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

    void setPattern(const QString &findText, const QString &replaceText, bool useRegex) {
        findInput->setText(findText);
        replaceInput->setText(replaceText);
        if (regexCheckBox) regexCheckBox->setChecked(useRegex);
    }

private slots:
    void findNext() {
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

    void replaceCurrent() {
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

    void replaceAll() {
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

private:
    QTextEdit *editor;
    QLineEdit *findInput;
    QLineEdit *replaceInput;
    QCheckBox *regexCheckBox;
};

class RegexHelpDialog : public QDialog {
    Q_OBJECT
public:
    RegexHelpDialog(EditorView *ed, QWidget *parent = nullptr) : QDialog(parent), editor(ed) {
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

private slots:
    void applySelected() {
        int row = tableWidget->currentRow();
        if (row < 0) return;
        QString findStr = tableWidget->item(row, 1)->text();
        QString repStr = tableWidget->item(row, 2)->text();

        accept(); // Close cheat sheet dialog

        // Open Find & Replace dialog pre-filled
        FindReplaceDialog *dlg = new FindReplaceDialog(editor, parentWidget());
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setPattern(findStr, repStr, true);
        dlg->show();
    }

private:
    EditorView *editor;
    QTableWidget *tableWidget;
};

class MicroWriter : public QMainWindow {
    Q_OBJECT
public:
    MicroWriter(QWidget *parent = nullptr) : QMainWindow(parent), currentLayoutMode(NormalLayout), darkModeActive(false) {
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
                connect(ed, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateStats);
            }
        });

        createMenus();
        createToolBar();
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

protected:
    void closeEvent(QCloseEvent *event) override {
        for (int i = 0; i < tabWidget->count(); ++i) {
            EditorView *ed = qobject_cast<EditorView*>(tabWidget->widget(i));
            if (ed && !maybeSave(ed)) {
                event->ignore();
                return;
            }
        }
        event->accept();
    }

private slots:
    void newFile(const QString &title = "Untitled") {
        EditorView *editor = new EditorView(this);
        connect(editor, &QTextEdit::textChanged, this, &MicroWriter::updateStats);
        connect(editor, &QTextEdit::cursorPositionChanged, this, &MicroWriter::updateStats);
        
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
    }

    void openFile() {
        QString fileName = QFileDialog::getOpenFileName(this, "Open Document", "", "Documents (*.html *.htm *.md *.txt);;All Files (*.*)");
        if (!fileName.isEmpty()) {
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
                int index = tabWidget->addTab(editor, fileInfo.fileName());
                tabWidget->setCurrentIndex(index);
                ruler->setEditor(editor);
                applyLayoutModeToEditor(editor, currentLayoutMode);
            }
        }
    }

    void saveFile() {
        EditorView *editor = currentEditor();
        if (!editor) return;

        if (editor->filePath.isEmpty()) {
            saveAsFile();
        } else {
            writeFile(editor->filePath);
        }
    }

    void saveAsFile() {
        EditorView *editor = currentEditor();
        if (!editor) return;

        QFileDialog dialog(this, "Save Document As");
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

    void closeTabRequested(int index) {
        EditorView *editor = qobject_cast<EditorView*>(tabWidget->widget(index));
        if (editor && maybeSave(editor)) {
            tabWidget->removeTab(index);
            delete editor;
            if (tabWidget->count() == 0) {
                newFile();
            }
        }
    }

    bool maybeSave(EditorView *editor) {
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

    EditorView* currentEditor() {
        return qobject_cast<EditorView*>(tabWidget->currentWidget());
    }

    void printDocument() {
        EditorView *editor = currentEditor();
        if (!editor) return;

        QPrinter printer(QPrinter::HighResolution);
        QPrintDialog dialog(&printer, this);
        if (dialog.exec() == QDialog::Accepted) {
            if (!editor->headerText().isEmpty() || !editor->footerText().isEmpty()) {
                QPainter painter(&printer);
                editor->document()->print(&printer);
                QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
                painter.setFont(QFont("Arial", 10));
                painter.setPen(Qt::black);
                if (!editor->headerText().isEmpty()) {
                    painter.drawText(pageRect.left() + 50, pageRect.top() + 50, editor->headerText());
                }
                if (!editor->footerText().isEmpty()) {
                    painter.drawText(pageRect.left() + 50, pageRect.bottom() - 50, editor->footerText());
                }
            } else {
                editor->print(&printer);
            }
        }
    }

    void printPreview() {
        EditorView *editor = currentEditor();
        if (!editor) return;

        QPrinter printer(QPrinter::HighResolution);
        QPrintPreviewDialog preview(&printer, this);
        connect(&preview, &QPrintPreviewDialog::paintRequested, this, [editor](QPrinter *prn) {
            editor->print(prn);
        });
        preview.exec();
    }

    void exportPdf() {
        EditorView *editor = currentEditor();
        if (!editor) return;

        QString fileName = QFileDialog::getSaveFileName(this, "Export PDF", "", "PDF Files (*.pdf)");
        if (!fileName.isEmpty()) {
            if (!fileName.endsWith(".pdf", Qt::CaseInsensitive)) {
                fileName += ".pdf";
            }
            QPrinter printer(QPrinter::HighResolution);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(fileName);
            if (!editor->headerText().isEmpty() || !editor->footerText().isEmpty()) {
                QPainter painter(&printer);
                editor->document()->print(&printer);
                QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
                painter.setFont(QFont("Arial", 10));
                painter.setPen(Qt::black);
                if (!editor->headerText().isEmpty()) {
                    painter.drawText(pageRect.left() + 50, pageRect.top() + 50, editor->headerText());
                }
                if (!editor->footerText().isEmpty()) {
                    painter.drawText(pageRect.left() + 50, pageRect.bottom() - 50, editor->footerText());
                }
            } else {
                editor->print(&printer);
            }
        }
    }

    void exportOdt() {
        EditorView *editor = currentEditor();
        if (!editor) return;

        QString fileName = QFileDialog::getSaveFileName(this, "Export as ODT", "", "OpenDocument Text (*.odt)");
        if (!fileName.isEmpty()) {
            if (!fileName.endsWith(".odt", Qt::CaseInsensitive)) {
                fileName += ".odt";
            }
            QTextDocumentWriter writer(fileName, "odf");
            writer.write(editor->document());
        }
    }

    void insertImage() {
        EditorView *editor = currentEditor();
        if (!editor) return;

        QString fileName = QFileDialog::getOpenFileName(this, "Insert Image", "", "Images (*.png *.jpg *.jpeg *.bmp *.gif)");
        if (!fileName.isEmpty()) {
            QImage image(fileName);
            if (!image.isNull()) {
                editor->textCursor().insertImage(image);
            }
        }
    }

    void insertBulletList() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCursor cursor = editor->textCursor();
        QTextListFormat listFormat;
        listFormat.setStyle(QTextListFormat::ListDisc);
        cursor.createList(listFormat);
    }

    void insertNumberedList() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCursor cursor = editor->textCursor();
        QTextListFormat listFormat;
        listFormat.setStyle(QTextListFormat::ListDecimal);
        cursor.createList(listFormat);
    }

    void insertHorizontalLine() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        editor->textCursor().insertHtml("<hr>");
    }

    void insertTable() {
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

    void insertDateTime() {
        EditorView *editor = currentEditor();
        if (!editor) return;

        DateTimeDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            QString formatted = QDateTime::currentDateTime().toString(dlg.selectedFormatString());
            editor->textCursor().insertText(formatted);
        }
    }

    void insertLink() {
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

    void insertSymbol() {
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

    void configureHeaderFooter() {
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

    void textShading() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QColor col = QColorDialog::getColor(Qt::yellow, this, "Choose Text Shading Color");
        if (col.isValid()) {
            QTextCharFormat fmt;
            fmt.setBackground(col);
            mergeFormatOnWordOrSelection(fmt);
            editor->setCurrentCharFormat(fmt);
        }
    }

    void textStrikethrough() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCharFormat fmt;
        fmt.setFontStrikeOut(!editor->textCursor().charFormat().fontStrikeOut());
        mergeFormatOnWordOrSelection(fmt);
        editor->setCurrentCharFormat(fmt);
    }

    void textSuperscript() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCharFormat fmt;
        int align = editor->textCursor().charFormat().verticalAlignment();
        if (align == QTextCharFormat::AlignSuperScript)
            fmt.setVerticalAlignment(QTextCharFormat::AlignNormal);
        else
            fmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
        mergeFormatOnWordOrSelection(fmt);
        editor->setCurrentCharFormat(fmt);
    }

    void textSubscript() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCharFormat fmt;
        int align = editor->textCursor().charFormat().verticalAlignment();
        if (align == QTextCharFormat::AlignSubScript)
            fmt.setVerticalAlignment(QTextCharFormat::AlignNormal);
        else
            fmt.setVerticalAlignment(QTextCharFormat::AlignSubScript);
        mergeFormatOnWordOrSelection(fmt);
        editor->setCurrentCharFormat(fmt);
    }

    void insertBlockquote() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCursor cursor = editor->textCursor();
        cursor.insertHtml("<blockquote style=\"border-left: 3px solid #007acc; margin: 10px 0; padding-left: 10px; color: #555;\"><p>Type blockquote text here...</p></blockquote>");
    }

    void toggleDoubleSpacing(bool doubleSpace) {
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

    // Line / Text Modification Helper
    void modifySelectedLines(std::function<QStringList(QStringList)> transformer) {
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

    // Format Menu Actions
    void formatIndent() {
        modifySelectedLines([](QStringList lines) {
            QStringList out;
            for (const QString &l : lines) out.append("    " + l);
            return out;
        });
    }

    void formatUnindent() {
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

    void formatStripBlankLines() {
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

    // Tools Menu Actions Implementation
    void toolsReverseLines() {
        modifySelectedLines([](QStringList lines) {
            std::reverse(lines.begin(), lines.end());
            return lines;
        });
    }

    void toolsSortLines() {
        modifySelectedLines([](QStringList lines) {
            std::sort(lines.begin(), lines.end(), [](const QString &a, const QString &b) {
                return QString::compare(a, b, Qt::CaseInsensitive) < 0;
            });
            return lines;
        });
    }

    void toolsUppercase() {
        modifySelectedLines([](QStringList lines) {
            QStringList out;
            for (const QString &l : lines) out.append(l.toUpper());
            return out;
        });
    }

    void toolsLowercase() {
        modifySelectedLines([](QStringList lines) {
            QStringList out;
            for (const QString &l : lines) out.append(l.toLower());
            return out;
        });
    }

    void toolsTitleCase() {
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

    void toolsAddPrefix() {
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

    void toolsAddSuffix() {
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

    void toolsTabsToSpaces() {
        modifySelectedLines([](QStringList lines) {
            QStringList out;
            for (QString l : lines) {
                out.append(l.replace('\t', "    "));
            }
            return out;
        });
    }

    void toolsSpacesToTabs() {
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

    void setLayoutMode(LayoutMode mode) {
        currentLayoutMode = mode;
        for (int i = 0; i < tabWidget->count(); ++i) {
            EditorView *ed = qobject_cast<EditorView*>(tabWidget->widget(i));
            applyLayoutModeToEditor(ed, mode);
        }
    }

    void applyLayoutModeToEditor(EditorView *editor, LayoutMode mode) {
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

    void toggleDarkMode(bool dark) {
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

    void textBold() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCharFormat fmt;
        fmt.setFontWeight(editor->fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
        mergeFormatOnWordOrSelection(fmt);
        editor->setCurrentCharFormat(fmt);
    }

    void textItalic() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCharFormat fmt;
        fmt.setFontItalic(!editor->fontItalic());
        mergeFormatOnWordOrSelection(fmt);
        editor->setCurrentCharFormat(fmt);
    }

    void textUnderline() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCharFormat fmt;
        fmt.setFontUnderline(!editor->fontUnderline());
        mergeFormatOnWordOrSelection(fmt);
        editor->setCurrentCharFormat(fmt);
    }

    void textTextColor() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QColor col = QColorDialog::getColor(editor->textColor(), this, "Choose Text Color");
        if (col.isValid()) {
            QTextCharFormat fmt;
            fmt.setForeground(col);
            mergeFormatOnWordOrSelection(fmt);
            editor->setCurrentCharFormat(fmt);
        }
    }

    void fontfamily(const QFont &font) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCharFormat fmt;
        fmt.setFontFamilies(QStringList{font.family()});
        mergeFormatOnWordOrSelection(fmt);
        editor->setCurrentCharFormat(fmt);
    }

    void fontSize(int size) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCharFormat fmt;
        fmt.setFontPointSize(size);
        mergeFormatOnWordOrSelection(fmt);
        editor->setCurrentCharFormat(fmt);
    }

    void alignLeft() { if (currentEditor()) currentEditor()->setAlignment(Qt::AlignLeft); }
    void alignCenter() { if (currentEditor()) currentEditor()->setAlignment(Qt::AlignCenter); }
    void alignRight() { if (currentEditor()) currentEditor()->setAlignment(Qt::AlignRight); }
    void alignJustify() { if (currentEditor()) currentEditor()->setAlignment(Qt::AlignJustify); }

    void showFindDialog() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        FindReplaceDialog *dlg = new FindReplaceDialog(editor, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    }

    void updateStats() {
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

    void updateWindowTitleAndStats() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        if (editor->filePath.isEmpty()) {
            setWindowTitle("Qt6 MicroWriter - New File");
        } else {
            setWindowTitle("Qt6 MicroWriter - " + editor->filePath);
        }
        updateStats();
    }

private:
    void writeFile(const QString &fileName) {
        EditorView *editor = currentEditor();
        if (!editor) return;

        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString content;
            if (fileName.endsWith(".md")) {
                content = editor->toMarkdown();
            } else if (fileName.endsWith(".html") || fileName.endsWith(".htm")) {
                content = editor->toHtml();
                // Modern HTML5 DOCTYPE fix
                content.replace(
                    QRegularExpression("<!DOCTYPE[^>]*>", QRegularExpression::CaseInsensitiveOption),
                    "<!DOCTYPE html>"
                );
            } else {
                content = editor->toPlainText();
            }

            content.replace(QRegularExpression("\r\n|\r"), "\n");
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

    void mergeFormatOnWordOrSelection(const QTextCharFormat &format) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        QTextCursor cursor = editor->textCursor();
        if (!cursor.hasSelection())
            cursor.select(QTextCursor::WordUnderCursor);
        cursor.mergeCharFormat(format);
        editor->mergeCurrentCharFormat(format);
    }

    void createMenus() {
        QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

        QAction *newAction = new QAction(tr("&New"), this);
        newAction->setShortcut(QKeySequence::New);
        connect(newAction, &QAction::triggered, this, [=](){ newFile(); });
        fileMenu->addAction(newAction);

        QAction *openAction = new QAction(tr("&Open..."), this);
        openAction->setShortcut(QKeySequence::Open);
        connect(openAction, &QAction::triggered, this, &MicroWriter::openFile);
        fileMenu->addAction(openAction);

        QAction *saveAction = new QAction(tr("&Save"), this);
        saveAction->setShortcut(QKeySequence::Save);
        connect(saveAction, &QAction::triggered, this, &MicroWriter::saveFile);
        fileMenu->addAction(saveAction);

        QAction *saveAsAct = new QAction(tr("Save &As..."), this);
        saveAsAct->setShortcut(QKeySequence::SaveAs);
        connect(saveAsAct, &QAction::triggered, this, &MicroWriter::saveAsFile);
        fileMenu->addAction(saveAsAct);

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

        QAction *printAct = new QAction(tr("&Print..."), this);
        printAct->setShortcut(QKeySequence::Print);
        connect(printAct, &QAction::triggered, this, &MicroWriter::printDocument);
        fileMenu->addAction(printAct);

        QAction *pdfAct = new QAction(tr("Export as &PDF..."), this);
        connect(pdfAct, &QAction::triggered, this, &MicroWriter::exportPdf);
        fileMenu->addAction(pdfAct);

        QAction *odtAct = new QAction(tr("Export as &ODT..."), this);
        connect(odtAct, &QAction::triggered, this, &MicroWriter::exportOdt);
        fileMenu->addAction(odtAct);

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

        QAction *redoAct = new QAction(tr("&Redo"), this);
        redoAct->setShortcut(QKeySequence::Redo);
        connect(redoAct, &QAction::triggered, this, [=](){ if (currentEditor()) currentEditor()->redo(); });
        editMenu->addAction(redoAct);

        editMenu->addSeparator();

        QAction *findAct = new QAction(tr("&Find & Replace..."), this);
        findAct->setShortcut(QKeySequence::Find);
        connect(findAct, &QAction::triggered, this, &MicroWriter::showFindDialog);
        editMenu->addAction(findAct);

        QMenu *formatMenu = menuBar()->addMenu(tr("&Format"));

        QAction *boldAct = new QAction(tr("&Bold"), this);
        boldAct->setShortcut(QKeySequence::Bold);
        connect(boldAct, &QAction::triggered, this, &MicroWriter::textBold);
        formatMenu->addAction(boldAct);

        QAction *italicAct = new QAction(tr("&Italic"), this);
        italicAct->setShortcut(QKeySequence::Italic);
        connect(italicAct, &QAction::triggered, this, &MicroWriter::textItalic);
        formatMenu->addAction(italicAct);

        QAction *underlineAct = new QAction(tr("&Underline"), this);
        underlineAct->setShortcut(QKeySequence::Underline);
        connect(underlineAct, &QAction::triggered, this, &MicroWriter::textUnderline);
        formatMenu->addAction(underlineAct);

        QAction *strikeAct = new QAction(tr("Strikethrough"), this);
        connect(strikeAct, &QAction::triggered, this, &MicroWriter::textStrikethrough);
        formatMenu->addAction(strikeAct);

        QAction *superAct = new QAction(tr("Superscript"), this);
        connect(superAct, &QAction::triggered, this, &MicroWriter::textSuperscript);
        formatMenu->addAction(superAct);

        QAction *subAct = new QAction(tr("Subscript"), this);
        connect(subAct, &QAction::triggered, this, &MicroWriter::textSubscript);
        formatMenu->addAction(subAct);

        formatMenu->addSeparator();

        QAction *colorAct = new QAction(tr("Text &Color..."), this);
        connect(colorAct, &QAction::triggered, this, &MicroWriter::textTextColor);
        formatMenu->addAction(colorAct);

        QAction *shadingAct = new QAction(tr("Text &Shading..."), this);
        connect(shadingAct, &QAction::triggered, this, &MicroWriter::textShading);
        formatMenu->addAction(shadingAct);

        formatMenu->addSeparator();

        QAction *leftAct = new QAction(tr("Align &Left"), this);
        connect(leftAct, &QAction::triggered, this, &MicroWriter::alignLeft);
        formatMenu->addAction(leftAct);

        QAction *centerAct = new QAction(tr("Align &Center"), this);
        connect(centerAct, &QAction::triggered, this, &MicroWriter::alignCenter);
        formatMenu->addAction(centerAct);

        QAction *rightAct = new QAction(tr("Align &Right"), this);
        connect(rightAct, &QAction::triggered, this, &MicroWriter::alignRight);
        formatMenu->addAction(rightAct);

        QAction *justifyAct = new QAction(tr("&Justify"), this);
        connect(justifyAct, &QAction::triggered, this, &MicroWriter::alignJustify);
        formatMenu->addAction(justifyAct);

        formatMenu->addSeparator();

        QAction *doubleSpaceAct = new QAction(tr("Double Spacing"), this);
        doubleSpaceAct->setCheckable(true);
        connect(doubleSpaceAct, &QAction::triggered, this, &MicroWriter::toggleDoubleSpacing);
        formatMenu->addAction(doubleSpaceAct);

        QAction *indentAct = new QAction(tr("Indent"), this);
        connect(indentAct, &QAction::triggered, this, &MicroWriter::formatIndent);
        formatMenu->addAction(indentAct);

        QAction *unindentAct = new QAction(tr("Unindent"), this);
        connect(unindentAct, &QAction::triggered, this, &MicroWriter::formatUnindent);
        formatMenu->addAction(unindentAct);

        QAction *stripBlankAct = new QAction(tr("Strip Blank Lines"), this);
        connect(stripBlankAct, &QAction::triggered, this, &MicroWriter::formatStripBlankLines);
        formatMenu->addAction(stripBlankAct);

        formatMenu->addSeparator();

        QAction *bulletAct = new QAction(tr("Bullet &List"), this);
        connect(bulletAct, &QAction::triggered, this, &MicroWriter::insertBulletList);
        formatMenu->addAction(bulletAct);

        QAction *numberedAct = new QAction(tr("&Numbered List"), this);
        connect(numberedAct, &QAction::triggered, this, &MicroWriter::insertNumberedList);
        formatMenu->addAction(numberedAct);

        QAction *blockquoteAct = new QAction(tr("Blockquote"), this);
        connect(blockquoteAct, &QAction::triggered, this, &MicroWriter::insertBlockquote);
        formatMenu->addAction(blockquoteAct);

        QAction *hrAct = new QAction(tr("Horizontal &Line"), this);
        connect(hrAct, &QAction::triggered, this, &MicroWriter::insertHorizontalLine);
        formatMenu->addAction(hrAct);

        QAction *tableAct = new QAction(tr("Insert &Table..."), this);
        connect(tableAct, &QAction::triggered, this, &MicroWriter::insertTable);
        formatMenu->addAction(tableAct);

        QAction *dateTimeAct = new QAction(tr("Insert &Date & Time..."), this);
        connect(dateTimeAct, &QAction::triggered, this, &MicroWriter::insertDateTime);
        formatMenu->addAction(dateTimeAct);

        QAction *linkAct = new QAction(tr("Insert &Link..."), this);
        connect(linkAct, &QAction::triggered, this, &MicroWriter::insertLink);
        formatMenu->addAction(linkAct);

        QAction *symbolAct = new QAction(tr("Insert &Symbol..."), this);
        connect(symbolAct, &QAction::triggered, this, &MicroWriter::insertSymbol);
        formatMenu->addAction(symbolAct);

        formatMenu->addSeparator();

        QAction *imgAct = new QAction(tr("Insert &Image..."), this);
        connect(imgAct, &QAction::triggered, this, &MicroWriter::insertImage);
        formatMenu->addAction(imgAct);

        QAction *headerFooterAct = new QAction(tr("Header & Footer..."), this);
        connect(headerFooterAct, &QAction::triggered, this, &MicroWriter::configureHeaderFooter);
        formatMenu->addAction(headerFooterAct);

        // Tools Menu
        QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));

        QAction *sortLinesAct = new QAction(tr("Sort Lines (Ignore Case)"), this);
        connect(sortLinesAct, &QAction::triggered, this, &MicroWriter::toolsSortLines);
        toolsMenu->addAction(sortLinesAct);

        QAction *reverseLinesAct = new QAction(tr("Reverse Lines"), this);
        connect(reverseLinesAct, &QAction::triggered, this, &MicroWriter::toolsReverseLines);
        toolsMenu->addAction(reverseLinesAct);

        toolsMenu->addSeparator();

        QMenu *caseMenu = toolsMenu->addMenu(tr("Fix Case"));
        QAction *upperAct = new QAction(tr("UPPERCASE"), this);
        connect(upperAct, &QAction::triggered, this, &MicroWriter::toolsUppercase);
        caseMenu->addAction(upperAct);

        QAction *lowerAct = new QAction(tr("lowercase"), this);
        connect(lowerAct, &QAction::triggered, this, &MicroWriter::toolsLowercase);
        caseMenu->addAction(lowerAct);

        QAction *titleAct = new QAction(tr("Title Case"), this);
        connect(titleAct, &QAction::triggered, this, &MicroWriter::toolsTitleCase);
        caseMenu->addAction(titleAct);

        toolsMenu->addSeparator();

        QAction *prefixAct = new QAction(tr("Add Prefix..."), this);
        connect(prefixAct, &QAction::triggered, this, &MicroWriter::toolsAddPrefix);
        toolsMenu->addAction(prefixAct);

        QAction *suffixAct = new QAction(tr("Add Suffix..."), this);
        connect(suffixAct, &QAction::triggered, this, &MicroWriter::toolsAddSuffix);
        toolsMenu->addAction(suffixAct);

        toolsMenu->addSeparator();

        QAction *tabsToSpacesAct = new QAction(tr("Convert Tabs to Spaces"), this);
        connect(tabsToSpacesAct, &QAction::triggered, this, &MicroWriter::toolsTabsToSpaces);
        toolsMenu->addAction(tabsToSpacesAct);

        QAction *spacesToTabsAct = new QAction(tr("Convert Spaces to Tabs"), this);
        connect(spacesToTabsAct, &QAction::triggered, this, &MicroWriter::toolsSpacesToTabs);
        toolsMenu->addAction(spacesToTabsAct);

        // View Menu
        QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

        QAction *toggleToolBarAct = new QAction(tr("Show Formatting Toolbar"), this);
        toggleToolBarAct->setCheckable(true);
        toggleToolBarAct->setChecked(true);
        connect(toggleToolBarAct, &QAction::triggered, this, [=](bool checked){
            if (formattingToolBar) formattingToolBar->setVisible(checked);
        });
        viewMenu->addAction(toggleToolBarAct);

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

    void createToolBar() {
        formattingToolBar = addToolBar(tr("Formatting"));

        QFontComboBox *fontCombo = new QFontComboBox(this);
        connect(fontCombo, &QFontComboBox::currentFontChanged, this, &MicroWriter::fontfamily);
        formattingToolBar->addWidget(fontCombo);

        QSpinBox *sizeSpin = new QSpinBox(this);
        sizeSpin->setRange(8, 72);
        sizeSpin->setValue(12);
        connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MicroWriter::fontSize);
        formattingToolBar->addWidget(sizeSpin);

        formattingToolBar->addSeparator();

        QAction *boldAct = new QAction(tr("&Bold"), this);
        boldAct->setShortcut(QKeySequence::Bold);
        connect(boldAct, &QAction::triggered, this, &MicroWriter::textBold);
        formattingToolBar->addAction(boldAct);

        QAction *italicAct = new QAction(tr("&Italic"), this);
        italicAct->setShortcut(QKeySequence::Italic);
        connect(italicAct, &QAction::triggered, this, &MicroWriter::textItalic);
        formattingToolBar->addAction(italicAct);

        QAction *underlineAct = new QAction(tr("&Underline"), this);
        underlineAct->setShortcut(QKeySequence::Underline);
        connect(underlineAct, &QAction::triggered, this, &MicroWriter::textUnderline);
        formattingToolBar->addAction(underlineAct);

        QAction *colorAct = new QAction(tr("&Color"), this);
        connect(colorAct, &QAction::triggered, this, &MicroWriter::textTextColor);
        formattingToolBar->addAction(colorAct);

        formattingToolBar->addSeparator();

        QAction *leftAct = new QAction(tr("Left"), this);
        connect(leftAct, &QAction::triggered, this, &MicroWriter::alignLeft);
        formattingToolBar->addAction(leftAct);

        QAction *centerAct = new QAction(tr("Center"), this);
        connect(centerAct, &QAction::triggered, this, &MicroWriter::alignCenter);
        formattingToolBar->addAction(centerAct);

        QAction *rightAct = new QAction(tr("Right"), this);
        connect(rightAct, &QAction::triggered, this, &MicroWriter::alignRight);
        formattingToolBar->addAction(rightAct);

        QAction *justifyAct = new QAction(tr("Justify"), this);
        connect(justifyAct, &QAction::triggered, this, &MicroWriter::alignJustify);
        formattingToolBar->addAction(justifyAct);
    }

    void createStatusBar() {
        statusLabel = new QLabel(this);
        statusBar()->addPermanentWidget(statusLabel);
    }

    Ruler *ruler;
    QTabWidget *tabWidget;
    QToolBar *formattingToolBar;
    QLabel *statusLabel;
    LayoutMode currentLayoutMode;
    bool darkModeActive;
    QAction *darkAct;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    MicroWriter window;
    window.show();
    
    return app.exec();
}

#include "main.moc"