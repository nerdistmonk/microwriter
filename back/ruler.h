#ifndef RULER_H
#define RULER_H

#include <QWidget>

class EditorView;
class QPaintEvent;
class QMouseEvent;
class QResizeEvent;

class Ruler : public QWidget {
    Q_OBJECT
public:
    explicit Ruler(QWidget *parent = nullptr);

    void setEditor(EditorView *ed);
    void syncWithEditor();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    EditorView *editor;
    int leftMarginPos;
    int rightMarginPos;
    bool draggingLeft;
    bool draggingRight;
};

#endif // RULER_H
