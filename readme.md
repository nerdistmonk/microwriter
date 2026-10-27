# Microwriter

**The diet soda of word processors, same refreshing taste, half the calories!**

**Version 2.0, released on October 27th, 2026**

***\*A lightweight, distraction-free rich text editor and code notepad built with
C++ and Qt. MicroWriter combines the simplicity of a classic notepad with
essential word processing tools, multi-tab editing, and versatile document
export capabilities. ***

<img src="shot.png" width="611" height="331" />
<img src="shot2.png" width="616" height="334" />

- - -
## Features

- **Multi-Tab Interface: Work on multiple documents simultaneously with per-tab
  layout memory.**
- **Rich Text Formatting: Bold, italic, underline, strikethrough, text color,
  highlight color, font family, and font size.**
- **Four Layout Modes: Switch seamlessly between Normal, Web, Print, and Code
  Layout modes.**
- **Dedicated Code Mode: Clean code editor with line numbering, bracket matching,
  syntax highlighting, and auto-detection for common source files.**
- **Context-Aware Menus: The Format menu automatically hides while working in
  Code Layout to keep the workspace clean and focused.**
- **Dual-Profile Customizable Toolbars: Separate, persistent toolbar setups for
  Rich Text and Code modes—right-click any toolbar to customize buttons, create
  new stacked rows, or remove bars.**
- **Universal Find and Replace: Fast text and regex search with match replacement
  across both rich text and code editors.**
- **Document Ruler: Visual interactive ruler for margins and tabs.**
- **Live Statistics: Real-time character count, word count, line/column tracking,
  encoding display, and newline mode detection (CRLF, LF, CR).**
- **Dark Mode Support: Clean light and dark theme toggling.**
- **Multi-Format Support: Open and save Markdown (`.md`), HTML (`.html`), plain
  text (`.txt`), and source code files, plus export to PDF and ODT.**

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


|Shortcut                   |Action          |
|---------------------------|----------------|
|`Ctrl + Z`                 |**Undo**        |
|`Ctrl + Y`** / `Ctrl + Shift + Z`**|**Redo**        |
|`Ctrl + X`                 |**Cut**         |
|`Ctrl + C`                 |**Copy**        |
|`Ctrl + V`                 |**Paste**       |
|`Ctrl + A`                 |**Select all text**|
|`Ctrl + F`                 |**Find and Replace**|
|`Alt + Q`                  |**Next Tab**    |

### View & Layout Modes


|Shortcut|Action       |
|--------|-------------|
|`Ctrl + 1`|**Normal Layout**|
|`Ctrl + 2`|**Web Layout**|
|`Ctrl + 3`|**Print Layout**|
|`Ctrl + 4`|**Code Layout**|

### Text Formatting (Rich Text Modes)


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

