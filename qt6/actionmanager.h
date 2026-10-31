#ifndef ACTIONMANAGER_H
#define ACTIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include "types.h"

class MicroWriter;
class QMenuBar;
class QMenu;
class QToolBar;
class QAction;
class QActionGroup;

class ActionManager : public QObject {
    Q_OBJECT
public:
    explicit ActionManager(MicroWriter *window);
    void setupMenus(QMenuBar *menuBar);
    void setupDefaultToolBar(QToolBar *toolbar);

    const QMap<QString, QAction*>& actionMap() const { return m_actionMap; }
    const QList<QAction*>& allMenuActions() const { return m_allMenuActions; }
    void syncLayoutModeAction(LayoutMode mode);
    void setFormatMenuVisible(bool visible);
    void updateFormatMenuActions(bool isRich, bool isMarkdown = false);

private:
    void registerAction(QAction *act);

    MicroWriter *m_window;
    QMenu *m_formatMenu;
    QList<QAction*> m_allMenuActions;
    QMap<QString, QAction*> m_actionMap;
    QActionGroup *m_layoutGrp;
    QMap<LayoutMode, QAction*> m_layoutActions;
};

#endif // ACTIONMANAGER_H
