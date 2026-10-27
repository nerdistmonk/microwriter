#ifndef ACTIONMANAGER_H
#define ACTIONMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

class QMainWindow;
class QMenuBar;
class QToolBar;
class QAction;
class MicroWriter;

class ActionManager : public QObject {
    Q_OBJECT
public:
    explicit ActionManager(MicroWriter *window);

    void setupMenus(QMenuBar *menuBar);
    void setupDefaultToolBar(QToolBar *toolbar);

    const QList<QAction*>& allActions() const { return m_allMenuActions; }
    const QMap<QString, QAction*>& actionMap() const { return m_actionMap; }

private:
    void registerAction(QAction *act);

    MicroWriter *m_window;
    QList<QAction*> m_allMenuActions;
    QMap<QString, QAction*> m_actionMap;
};

#endif // ACTIONMANAGER_H
