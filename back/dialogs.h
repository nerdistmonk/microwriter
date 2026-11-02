#ifndef DIALOGS_H
#define DIALOGS_H

#include <QDialog>
#include <QString>
#include <QList>
#include <Qt>

class QTextEdit;
class EditorView;
class BaseEditor;
class QToolBar;
class QAction;
class QSpinBox;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QTableWidget;
class QListWidget;

class TableDialog : public QDialog {
    Q_OBJECT
public:
    explicit TableDialog(QWidget *parent = nullptr);
    int rows() const;
    int cols() const;
    Qt::Alignment alignment() const;
private:
    QSpinBox *rowSpin;
    QSpinBox *colSpin;
    QComboBox *alignCombo;
};

class DateTimeDialog : public QDialog {
    Q_OBJECT
public:
    explicit DateTimeDialog(QWidget *parent = nullptr);
    QString selectedFormatString() const;
private:
    QComboBox *formatCombo;
};

class LinkDialog : public QDialog {
    Q_OBJECT
public:
    explicit LinkDialog(const QString &defaultText = "", QWidget *parent = nullptr);
    QString url() const;
    QString linkText() const;
private:
    QLineEdit *urlInput;
    QLineEdit *textInput;
};

class SymbolDialog : public QDialog {
    Q_OBJECT
public:
    explicit SymbolDialog(QWidget *parent = nullptr);
    QString getSelectedSymbol() const;
private:
    QString selectedSymbol;
};

class HeaderFooterDialog : public QDialog {
    Q_OBJECT
public:
    HeaderFooterDialog(const QString &header, const QString &footer, QWidget *parent = nullptr);
    QString getHeader() const;
    QString getFooter() const;
private:
    QLineEdit *headerInput;
    QLineEdit *footerInput;
};

class FindReplaceDialog : public QDialog {
    Q_OBJECT
public:
    explicit FindReplaceDialog(BaseEditor *ed, QWidget *parent = nullptr);
    void setPattern(const QString &findText, const QString &replaceText, bool useRegex);

private slots:
    void findNext();
    void replaceCurrent();
    void replaceAll();

private:
    BaseEditor *editor;
    QLineEdit *findInput;
    QLineEdit *replaceInput;
    QCheckBox *regexCheckBox;
};

class RegexHelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit RegexHelpDialog(EditorView *ed, QWidget *parent = nullptr);

private slots:
    void applySelected();

private:
    EditorView *editor;
    QTableWidget *tableWidget;
};

class HtmlHelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HtmlHelpDialog(BaseEditor *ed, QWidget *parent = nullptr);

private slots:
    void applySelected();

private:
    BaseEditor *editor;
    QTableWidget *tableWidget;
};

class SedHelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit SedHelpDialog(BaseEditor *ed, QWidget *parent = nullptr);

private slots:
    void applySelected();

private:
    BaseEditor *editor;
    QTableWidget *tableWidget;
};

class BashHelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit BashHelpDialog(BaseEditor *ed, QWidget *parent = nullptr);

private slots:
    void applySelected();

private:
    BaseEditor *editor;
    QTableWidget *tableWidget;
};

class MarkdownHelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit MarkdownHelpDialog(BaseEditor *editor, QWidget *parent = nullptr);
private slots:
    void applySelected();
private:
    BaseEditor *editor;
    QTableWidget *tableWidget;
};

class ToolbarCustomizeDialog : public QDialog {
    Q_OBJECT
public:
    ToolbarCustomizeDialog(QToolBar *targetToolbar, const QList<QAction*> &allActions, QWidget *parent = nullptr);

private:
    void populateLists();
    void addActionToToolbar();
    void removeActionFromToolbar();

    QToolBar *toolbar;
    QList<QAction*> availableActions;
    QListWidget *availableList;
    QListWidget *activeList;
};

#endif // DIALOGS_H