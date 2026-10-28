# MicroWriter TUI

**The diet soda of word processors, now in a refreshing display server free
flavor!**

**Version 2.5, released on October 28th, 2026**

**A lightweight, distraction-free terminal text editor and code notepad built
with C++ and ncurses. MicroWriter TUI provides complete terminal parity with
the Qt6 GUI version, combining snappy multi-tab editing, rich text markup
tools, dedicated code development features, and native Wayland/X11 clipboard
interoperability.**

<img src="shot3.png" width="773" height="427" />

- - -
## Features

- **Multi-Tab Interface: Work on multiple documents simultaneously with
  mouse-friendly tab switching, double-click tab creation, and right-click tab
  save/close menus.**
- **Rich Text & Code Layout Parity: Switch seamlessly between Rich Text and
  Code Layout modes with contextual toolbar and menu adaptation.**
- **Dedicated Code Mode: Gutter line numbers, rainbow bracket matching,
  real-time syntax tinting, filetype-aware comment toggling (`//` vs `\#`),
  smart return indentation, and tab-boundary backspacing.**
- **Mouse & Wheel Support: Full terminal mouse integration for drag-to-select,
  clickable menus, tab switching, tool buttons, and smooth mouse-wheel
  scrolling across buffers and modal dialogs.**
- **System Clipboard Bridging: Transparent bidirectional clipboard
  synchronization with Wayland (`wl-copy` / `wl-paste`) and X11 (`xclip` / `xsel`
  ) so copying or pasting to and from GUI apps works out of the box.**
- **Universal Find and Replace: Powerful regex engine (ECMAScript syntax with
  capture groups `$1`, `$2`) and literal search that respects markup boundaries
  in rich text documents.**
- **Dynamic System Fonts: Live font enumeration via `fontconfig` (`fc-list`)
  allowing selection of any installed system typeface with wheel-scrollable
  selection dialogs.**
- **HTML Document Fidelity: Seamless save/load compatibility with Qt6 rich text
  files, preserving paragraphs, formatting tags, line breaks, and whitespace
  structure.**
- **Built-in Developer Reference: Instant-access cheat sheets in the Help menu
  for Regex patterns, HTML quick references, Sed/Unix pipeline utilities (`sed`, `
  awk`, `grep`, `cut`, `xargs`), and reusable Bash script boilerplates.**
- **Visual Feedback: Instant highlight flash on the Save button upon writing to
  disk and a full-bleed status bar with live word, character, line, column,
  encoding, and line-ending (LF, CRLF, CR) counters.**

- - -
## Command-Line Usage

**Launch directly into the console TUI or force code editing mode:**

**\# Launch in terminal mode**

**microwriter -c \[file1] \[file2] ...**

**microwriter --console \[file1] \[file2] ...**

**\# Launch directly in Code Layout mode**

**microwriter -C \[file1] \[file2] ...**

**microwriter --code \[file1] \[file2] ...**

***(Running without console flags opens the standard Qt6 GUI version).***

- - -
## Keyboard Shortcuts

### Function Key System (F1 – F12)


|Shortcut|Action                             |
|--------|-----------------------------------|
|`F1`    |**Help & About Dialog**            |
|`F2`    |**New Tab / Document**             |
|`F3`    |**Cycle Through Tabs**             |
|`F4`    |**Find & Replace (Regex / Plain Text)**|
|`F5`    |**Switch to Rich Text Layout**     |
|`F6`    |**Switch to Code Layout**          |
|`F7`    |**Open File Dialog**               |
|`F8`    |**Save Current File**              |
|`F9`    |**Export Document as PDF**         |
|`F10`   |**Export Document as ODF**         |
|`F11`   |**Activate Dropdown Menu Bar**     |
|`F12`   |**Quit / Exit MicroWriter**        |

### File & App Operations


|Shortcut|Action                                              |
|--------|----------------------------------------------------|
|`Ctrl + S`|**Save Current File (Flashes Save indicator)**      |
|`Ctrl + Q`|**Quit / Exit MicroWriter (Checks for unsaved changes)**|

### Editing & Clipboard


|Shortcut             |Action                                               |
|---------------------|-----------------------------------------------------|
|`Ctrl + X`           |**Cut Selection / Line (Syncs to system clipboard)** |
|`Ctrl + C`           |**Copy Selection / Line (Syncs to system clipboard)**|
|`Ctrl + V`           |**Paste (Reads from system clipboard: `wl-paste` / `xclip`)**|
|`Ctrl + D`           |**Nano-style Cut Entire Line**                       |
|`Ctrl + A`           |**Select All Text**                                  |
|`Ctrl + E`           |**Undo**                                             |
|`Ctrl + R`           |**Redo**                                             |
|`Tab`                |**Indent 4 Spaces / Block Indent Selection**         |
|`Shift + Tab`** (`Backtab`)**|**Unindent Line / Selection**                        |

### Navigation & Dialogs


|Shortcut   |Action                                          |
|-----------|------------------------------------------------|
|`Arrows`   |**Navigate cursor**                             |
|`PgUp / PgDn`|**Scroll page up / down**                       |
|`Home / End`|**Jump to line start / end**                    |
|`Esc`      |**Cancel active modal prompt, cheat sheet, or menu**|

### Mouse Actions


|Mouse Event |Target            |Action                                               |
|------------|------------------|-----------------------------------------------------|
|**Click**   |**Menu Bar (Row 0)**|**Open dropdown menus**                              |
|**Click**   |**Tab Bar (Row 1)**|**Switch active tab**                                |
|**Double-Click**|**Tab Bar (Row 1)**|**Create new tab**                                   |
|**Right-Click**|**Tab (Row 1)**   |**Tab Context Menu (Save, Close, Save & Close)**     |
|**Click**   |**Toolbar (Row 2)**|**Quick actions (Save, PDF, Open, Formatting, Comments)**|
|**Click / Drag**|**Content Area**  |**Position cursor or create text selection**         |
|**Click**   |**Gutter (Code Mode)**|**Select entire line**                               |
|**Mouse Wheel**|**Editor / Dialogs**|**Scroll document viewport or modal lists smoothly** |

- - -
## Build Requirements

- **C++17 compatible compiler (GCC or Clang)**
- **CMake 3.16 or higher**
- **ncurses library (with wide-character/mouse support)**
- **fontconfig (recommended for full system font discovery)**
- **wl-clipboard (`wl-copy`/`wl-paste`) or xclip / xsel (for external clipboard
  integration)**
- **Qt 6 (Core, Gui, Widgets, PrintSupport - for shared build target)**

### Building from Source

**mkdir build && cd build**

**cmake ..**

**cmake --build .**

**To run the TUI editor:**

**./microwriter -c**

