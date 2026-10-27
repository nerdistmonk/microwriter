#ifndef TYPES_H
#define TYPES_H

enum LayoutMode {
    NormalLayout,
    WebLayout,
    PrintLayout
};

enum EolFormat {
    EolLf,   // Unix (\n)
    EolCrlf, // Windows (\r\n)
    EolCr    // Mac Classic (\r)
};

#endif // TYPES_H
