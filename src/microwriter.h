#ifndef MICROWRITER_H
#define MICROWRITER_H

#include <QMainWindow>
#include <QList>
#include <QMap>
#include <functional>
#include "types.h"

class EditorView;
class Ruler;
class QTabWidget;
class QLabel;
class QToolBar;
class QAction;
class QCloseEvent;
class QMenu;
class QTextCharFormat;
class QFontComboBox;
class QSpinBox;

class MicroWriter : public QMainWindow {
    Q_OBJECT
public:
    explicit MicroWriter(QWidget *parent = nullptr);
    void openFilePath(const QString &fileName);

protected:
    void closeEvent(QCloseEvent *event) override;
    QMenu* createPopupMenu() override;

private slots:
    void createNewToolbar();
    void newFile(const QString &title = "Untitled");
    void openFile();
    void saveFile();
    void saveAsFile();
    void closeTabRequested(int index);

    void printDocument();
    void printPreview();
    void exportPdf();
    void exportOdt();

    void insertImage();
    void insertBulletList();
    void insertNumberedList();
    void insertHorizontalLine();
    void insertTable();
    void insertDateTime();
    void insertLink();
    void insertSymbol();
    void configureHeaderFooter();

    void textShading();
    void textStrikethrough();
    void textSuperscript();
    void textSubscript();
    void insertBlockquote();
    void toggleDoubleSpacing(bool doubleSpace);

    void formatIndent();
    void formatUnindent();
    void formatStripBlankLines();

    void toolsReverseLines();
    void toolsSortLines();
    void toolsUppercase();
    void toolsLowercase();
    void toolsTitleCase();
    void toolsAddPrefix();
    void toolsAddSuffix();
    void toolsTabsToSpaces();
    void toolsSpacesToTabs();

    void setLayoutMode(LayoutMode mode);
    void toggleDarkMode(bool dark);

    void textBold();
    void textItalic();
    void textUnderline();
    void textTextColor();
    void fontfamily(const QFont &font);
    void fontSize(int size);

    void alignLeft();
    void alignCenter();
    void alignRight();
    void alignJustify();

    void showFindDialog();
    void updateStats();
    void updateWindowTitleAndStats();
    void updateFormatToolbarState();

private:
    void createMenus();
    void createDefaultToolBar();
    void createStatusBar();
    void saveToolbarSettings();
    void loadToolbarSettings();
    bool maybeSave(EditorView *editor);
    EditorView* currentEditor();
    void writeFile(const QString &fileName);
    void mergeFormatOnWordOrSelection(const QTextCharFormat &format);
    void registerAction(QAction *act);
    void modifySelectedLines(std::function<QStringList(QStringList)> transformer);
    void applyLayoutModeToEditor(EditorView *editor, LayoutMode mode);

    Ruler *ruler;
    QTabWidget *tabWidget;
    QLabel *statusLabel;
    QFontComboBox *fontCombo;
    QSpinBox *sizeSpin;
    LayoutMode currentLayoutMode;
    bool darkModeActive;
    QAction *darkAct;
    QList<QAction*> allMenuActions;
    QMap<QString, QAction*> actionMap;
    QList<QToolBar*> toolbars;
};

#endif // MICROWRITER_H
