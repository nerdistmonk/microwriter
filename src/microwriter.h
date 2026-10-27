#ifndef MICROWRITER_H
#define MICROWRITER_H

#include <QMainWindow>
#include <QList>
#include "types.h"
#include "baseeditor.h"

class EditorView;
class Ruler;
class ActionManager;
class QTabWidget;
class QLabel;
class QToolBar;
class QAction;
class QCloseEvent;
class QMenu;
class QFontComboBox;
class QSpinBox;

class MicroWriter : public QMainWindow {
    Q_OBJECT
public:
    explicit MicroWriter(QWidget *parent = nullptr);
    void openFilePath(const QString &fileName);

    BaseEditor* currentBaseEditor() const;
    EditorView* currentEditor() const;

    void updateFormatToolbarState();
    void setDarkModeAction(QAction *act) { darkAct = act; }

public slots:
    void newFile(const QString &title = "Untitled");
    void openFile();
    void saveFile();
    void saveAsFile();
    void closeTabRequested(int index);

    void setLayoutMode(LayoutMode mode);
    void toggleDarkMode(bool dark);
    void fontfamily(const QFont &font);
    void fontSize(int size);

    void createNewToolbar();

protected:
    void closeEvent(QCloseEvent *event) override;
    QMenu* createPopupMenu() override;

private:
    void createDefaultToolBar();
    void createStatusBar();
    void saveToolbarSettings();
    void loadToolbarSettings();
    bool maybeSaveTab(int index);
    void writeFile(const QString &fileName);
    void applyLayoutModeToEditor(EditorView *editor, LayoutMode mode);
    void updateStats();
    void updateWindowTitleAndStats();
    void attachEditorSignals(BaseEditor *editor);

    Ruler *ruler;
    QTabWidget *tabWidget;
    QLabel *statusLabel;
    QFontComboBox *fontCombo;
    QSpinBox *sizeSpin;
    ActionManager *actionManager;

    LayoutMode currentLayoutMode;
    bool darkModeActive;
    QAction *darkAct;
    QList<QToolBar*> toolbars;
};

#endif // MICROWRITER_H
