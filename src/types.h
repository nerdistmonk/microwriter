#ifndef TYPES_H
#define TYPES_H

#include <QString>

enum LayoutMode {
    NormalLayout,
    WebLayout,
    PrintLayout,
    CodeLayout
};

enum EolFormat {
    EolLf,   // Unix (\n)
    EolCrlf, // Windows (\r\n)
    EolCr    // Mac Classic (\r)
};

struct EditorStats {
    int line = 1;
    int column = 1;
    int words = 0;
    int characters = 0;
    QString encoding = "UTF-8";
    EolFormat eol = EolLf;
};

#endif // TYPES_H
