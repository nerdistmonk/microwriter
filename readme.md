# Microwriter

**The diet soda of word processors, same refreshing taste, half the calories!**

**Version 1.4, released on October 27th, 2026**

**\**A lightweight, distraction-free rich text editor built with C++ and Qt.
MicroWriter combines the simplicity of a classic notepad with essential word
processing tools, multi-tab editing, and versatile document export capabilities.*
\*<img src="shot.png" width="875" height="477" />**

- - -
## Features

- **Multi-Tab Interface: Work on multiple documents simultaneously.**
- **Rich Text Formatting: Bold, italic, underline, strikethrough, text color,
  highlight color, font family, and font size.**
- **Document Layouts: Switch seamlessly between Normal, Web, and Print layout
  modes.**
- **Document Ruler: Visual interactive ruler for margins and tabs.**
- **Live Statistics: Real-time character count, word count, line/column tracking,
  encoding display, and newline mode detection (CRLF, LF, CR).**
- **Dark Mode Support: Clean light and dark theme toggling.**
- **Multi-Format Support: Open and save Markdown (`.md`), HTML (`.html`), and
  plain text (`.txt`) files, plus export to PDF.**

- - -
## Keyboard Shortcuts

### File Operations


|Shortcut        |Action            |
|----------------|------------------|
|`Ctrl + N`      |**New tab / document**|
|`Ctrl + O`      |**Open file**     |
|`Ctrl + S`      |**Save current file**|
|`Ctrl + Shift + S`|**Save As**       |
|`Ctrl + W`      |**Close active tab**|
|`Ctrl + P`      |**Print document**|
|`Ctrl + Q`      |**Exit MicroWriter**|

### Editing & Navigation


|Shortcut                       |Action          |
|-------------------------------|----------------|
|`Ctrl + Z`                     |**Undo**        |
|`Ctrl + Y`**\** / `Ctrl + Shift + Z`\*\***|**Redo**        |
|`Ctrl + X`                     |**Cut**         |
|`Ctrl + C`                     |**Copy**        |
|`Ctrl + V`                     |**Paste**       |
|`Ctrl + A`                     |**Select all text**|
|`Ctrl + F`                     |**Find and Replace**|

### Text Formatting


|Shortcut        |Action       |
|----------------|-------------|
|`Ctrl + B`      |**Bold**     |
|`Ctrl + I`      |**Italic**   |
|`Ctrl + U`      |**Underline**|
|`Ctrl + Shift + X`|**Strikethrough**|
|`Ctrl + L`      |**Align Left**|
|`Ctrl + E`      |**Align Center**|
|`Ctrl + R`      |**Align Right**|
|`Ctrl + J`      |**Justify**  |

- - -
## Build Requirements

- **C++17 compatible compiler (GCC, Clang, or MSVC)**
- **CMake 3.16 or higher**
- **Qt 6 (Core, Gui, Widgets, PrintSupport)**

### Building from Source

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

