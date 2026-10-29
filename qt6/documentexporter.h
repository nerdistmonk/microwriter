#ifndef DOCUMENTEXPORTER_H
#define DOCUMENTEXPORTER_H

#include <QWidget>

class EditorView;

namespace DocumentExporter {

void printDocument(EditorView *editor, QWidget *parent);
void printPreview(EditorView *editor, QWidget *parent);
void exportPdf(EditorView *editor, QWidget *parent);
void exportOdt(EditorView *editor, QWidget *parent);

} // namespace DocumentExporter

#endif // DOCUMENTEXPORTER_H
