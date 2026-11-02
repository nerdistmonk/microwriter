#include "ruler.h"
#include "editorview.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPolygon>
#include <QtGlobal>

Ruler::Ruler(QWidget *parent)
    : QWidget(parent),
      editor(nullptr),
      leftMarginPos(0),
      rightMarginPos(700),
      draggingLeft(false),
      draggingRight(false)
{
    setFixedHeight(28);
    setMouseTracking(true);
}

void Ruler::setEditor(EditorView *ed) {
    editor = ed;
    syncWithEditor();
    update();
}

void Ruler::syncWithEditor() {
    if (!editor) return;
    leftMarginPos = qRound(editor->leftMarginPixels());
    rightMarginPos = width() - qRound(editor->rightMarginPixels());
    leftMarginPos = qBound(0, leftMarginPos, width() - 50);
    rightMarginPos = qBound(leftMarginPos + 40, rightMarginPos, width() - 5);
}

void Ruler::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    syncWithEditor();
}

void Ruler::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(240, 240, 240));

    painter.setPen(QColor(180, 180, 180));
    painter.drawLine(0, height() - 1, width(), height() - 1);

    const int dpi = 96;
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

void Ruler::mousePressEvent(QMouseEvent *event) {
    if (!editor) return;
    if (qAbs(event->pos().x() - leftMarginPos) < 10) {
        draggingLeft = true;
    } else if (qAbs(event->pos().x() - rightMarginPos) < 10) {
        draggingRight = true;
    }
}

void Ruler::mouseMoveEvent(QMouseEvent *event) {
    int x = event->pos().x();
    if (draggingLeft) {
        leftMarginPos = qBound(0, x, rightMarginPos - 40);
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

void Ruler::mouseReleaseEvent(QMouseEvent *) {
    draggingLeft = false;
    draggingRight = false;
}
