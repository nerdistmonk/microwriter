#ifndef DOCUMENTFORMATTER_H
#define DOCUMENTFORMATTER_H

#include <QWidget>
#include <QFont>

class EditorView;

namespace DocumentFormatter {

void toggleBold(EditorView *editor);
void toggleItalic(EditorView *editor);
void toggleUnderline(EditorView *editor);
void toggleStrikethrough(EditorView *editor);
void toggleSuperscript(EditorView *editor);
void toggleSubscript(EditorView *editor);

void applyTextColor(EditorView *editor, QWidget *parent);
void applyTextShading(EditorView *editor, QWidget *parent);
void applyFontFamily(EditorView *editor, const QFont &font);
void applyFontSize(EditorView *editor, int size);

void setAlignment(EditorView *editor, Qt::Alignment align);
void toggleDoubleSpacing(EditorView *editor, bool doubleSpace);

void insertBulletList(EditorView *editor);
void insertNumberedList(EditorView *editor);
void insertHorizontalLine(EditorView *editor);
void insertBlockquote(EditorView *editor);

void insertTable(EditorView *editor, QWidget *parent);
void insertDateTime(EditorView *editor, QWidget *parent);
void insertLink(EditorView *editor, QWidget *parent);
void insertSymbol(EditorView *editor, QWidget *parent);
void insertImage(EditorView *editor, QWidget *parent);
void configureHeaderFooter(EditorView *editor, QWidget *parent);

} // namespace DocumentFormatter

#endif // DOCUMENTFORMATTER_H
