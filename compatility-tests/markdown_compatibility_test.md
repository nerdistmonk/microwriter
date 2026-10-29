# H1 Header: MicroWriter Compatibility Suite
## H2 Header: Text Formatting & Inline Styles
### H3 Header: Lists & Task Lists

This is a paragraph featuring **bold text**, *italicized text*, ***bold and italicized***, ~~strikethrough~~, and `inline code` elements. You can also include hyperlinks like [MicroWriter on GitHub](https://github.com) and footnotes[^1].

---

## Blockquotes & Callouts

> This is a standard blockquote used to test multi-line quote rendering in the editor.
> It spans across two lines to ensure word-wrapping and border styling look correct.
> > Nested blockquotes are also important to verify indentation handling.

---

## Lists & Task Management

### Unordered List
* Item one with some extra text to wrap around.
* Item two
  * Sub-item A
  * Sub-item B
    * Deep sub-item

### Ordered List
1. First step in the compilation process.
2. Second step involving ncurses terminal layout rendering.
3. Final step: saving the file with the proper extension.

### Task List
- [x] Implement save/load modal dialogs
- [x] Add arrow key format cycling (Markdown/HTML)
- [ ] Fix edge cases in buffer reallocation
- [ ] Write comprehensive documentation

---

## Code Blocks & Syntax Highlighting

Here is a C code snippet simulating a simple buffer loop:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Loading MicroWriter v2.5B...\n");
    char *buffer = malloc(1024);
    if (buffer == NULL) {
        return 1;
    }
    free(buffer);
    return 0;
}
```

---

## Tables

| Feature | Status | Priority | Notes |
| :--- | :---: | ---: | :--- |
| Modal Dialogs | Done | High | Uses custom key handlers |
| Format Cycling | Done | Medium | Arrow keys / Tab support |
| Syntax Highlighting | In Progress | Low | Basic regex tokenization |

---

## Math Notation & Scientific Formulas

Inline mathematical notation is supported using standard delimiters, such as $E = mc^2$ for mass-energy equivalence. 

Display math equations can be rendered on their own lines using double delimiters:

$$\int_{0}^{\infty} e^{-x^2} dx = \frac{\sqrt{\pi}}{2}$$

---

## Footnotes

[^1]: This is a test footnote to verify reference link parsing and jump behaviors.