#ifndef MICROWRITER_TUITYPES_H
#define MICROWRITER_TUITYPES_H

#include <string>
#include <vector>

enum class TuiMode {
    RichText,
    Code
};

enum class TuiWrapMode {
    Off,
    Soft,
    Hard
};

struct TextPosition {
    int line = 0;
    int col = 0;
    bool operator==(const TextPosition &other) const { return line == other.line && col == other.col; }
    bool operator<(const TextPosition &other) const {
        if (line != other.line) return line < other.line;
        return col < other.col;
    }
};

struct DocSnapshot {
    std::vector<std::string> lines;
    TextPosition cursor;
};

struct EditorDocument {
    std::string filePath;
    std::string title = "Untitled";
    std::vector<std::string> lines = {""};
    bool modified = false;
    TuiMode mode = TuiMode::RichText;
    TuiWrapMode wrapMode = TuiWrapMode::Off;
    int wrapWidth = 0;
    std::string eolFormat = "LF";
    std::string encoding = "UTF-8";

    TextPosition cursor;
    TextPosition selectionAnchor;
    bool hasSelection = false;

    int scrollLine = 0;
    int scrollCol = 0;

    std::vector<DocSnapshot> undoStack;
    std::vector<DocSnapshot> redoStack;

    void pushUndo() {
        undoStack.push_back({lines, cursor});
        if (undoStack.size() > 50) {
            undoStack.erase(undoStack.begin());
        }
        redoStack.clear();
    }
};

#endif // MICROWRITER_TUITYPES_H
