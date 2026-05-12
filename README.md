# Kilo Text Editor

A minimal terminal-based text editor written in C. Single-file implementation (`kilo.c`) inspired by the classic [kilo](https://github.com/antirez/kilo) editor.

---

## Build & Run

```bash
make                 # compiles with gcc -Wall -Wextra -pedantic -std=c99
./kilo               # opens with a welcome screen
./kilo test.txt      # opens a file
```

Quit with `Ctrl+Q`.

---

## Keybindings

| Key | Action |
|-----|--------|
| Arrow keys | Move cursor |
| Page Up / Page Down | Scroll one screen |
| Home | Move cursor to start of line |
| End | Move cursor to end of line |
| Ctrl+Q | Quit |

---

## Code Structure — `kilo.c`

Everything is in one file. Sections appear in this order:

```
kilo.c
├── Defines & abuf     — constants, macros, append-buffer struct
├── Data               — editorConfig (global state), erow (one text row)
├── Terminal           — raw mode toggle, key reading
├── Row Operations     — appending rows from file data
├── File I/O           — opening and reading a file
├── Output             — scroll logic, screen drawing, screen refresh
├── Input              — cursor movement, keypress dispatch
└── Init / Main        — editor init, main loop
```

---

## How the Code Flows

### Startup

```
main()
  └─ enableRawMode()      — saves original terminal settings, switches to raw mode
  └─ initEditor()         — zeros out global E: cursor (cx=0,cy=0), rowoff=0,
  |                          numrows=0, row=NULL, gets terminal dimensions
  └─ editorOpen(filename) — if a file was passed as argv[1]
  └─ main loop (forever)
        └─ editorRefreshScreen()
        └─ editorProcessKeypress()
```

### Raw Mode

`enableRawMode()` calls `tcgetattr` to save the original terminal settings into `E.orig_termios`, then modifies a copy to:
- Turn off echo, canonical mode, signals (ISIG), and extension processing
- Disable output post-processing so `\n` isn't translated
- Set read timeout so `read()` doesn't block forever

`disableRawMode()` is registered with `atexit()` so the terminal is always restored on exit, even on crashes via `die()`.

### Reading Keypresses — `editorReadKey()`

Blocks in a loop calling `read()` until one byte arrives. If the byte is `\x1b` (escape), it reads two more bytes to detect arrow keys and special keys, which terminals send as escape sequences like `\x1b[A` (up arrow). These are mapped to the `editorKey` enum:

```
ARROW_UP / DOWN / LEFT / RIGHT
PAGE_UP / PAGE_DOWN
HOME_KEY / END_KEY
DEL_KEY
```

### Loading a File — `editorOpen()`

Opens the file with `fopen`, reads line-by-line using `getline()`, strips trailing `\r` and `\n`, then calls `editorAppendRow()` for each line.

`editorAppendRow()` grows `E.row` (the array of `erow` structs) with `realloc`, `malloc`s a buffer for the line content, copies it in, and increments `E.numrows`.

### The Main Loop

Every iteration:
1. `editorRefreshScreen()` — redraws the whole screen
2. `editorProcessKeypress()` — waits for one key and acts on it

### Screen Refresh — `editorRefreshScreen()`

Uses an **append buffer** (`abuf`) — a heap-allocated string that accumulates all output, then flushes it in a single `write()`. This avoids flicker from multiple small writes.

Steps:
1. `editorScroll()` — adjusts `E.rowoff` so the cursor stays visible
2. Hide cursor (`\x1b[?25l`)
3. Move cursor to top-left (`\x1b[H`)
4. `editorDrawRows()` — writes each visible line (or `~` for empty lines past EOF, welcome message at 1/3 screen height when no file is open)
5. Reposition cursor to `(E.cy - E.rowoff + 1, E.cx + 1)` using a VT100 escape
6. Show cursor (`\x1b[?25h`)
7. Flush the whole buffer with one `write()`

### Scrolling — `editorScroll()`

`E.rowoff` is the file-row index of the topmost visible line. This function checks:
- If cursor moved above the visible window → `rowoff = cy`
- If cursor moved below the visible window → `rowoff = cy - screenrows + 1`

All row rendering uses `filerow = y + E.rowoff` to translate screen rows to file rows.

### Cursor Movement — `editorMoveCursor()`

Moves `E.cx` / `E.cy` in response to arrow keys, clamped to screen/file bounds. `editorProcessKeypress()` also handles:
- `HOME_KEY` → `cx = 0`
- `END_KEY` → `cx = screencols - 1`
- `PAGE_UP/DOWN` → calls `editorMoveCursor(ARROW_UP/DOWN)` `screenrows` times

### Global State — `struct editorConfig E`

| Field | Meaning |
|-------|---------|
| `cx`, `cy` | Cursor column and row (file coordinates) |
| `rowoff` | First visible file row (scroll offset) |
| `screenrows` | Terminal height |
| `screencols` | Terminal width |
| `numrows` | Number of loaded rows |
| `row` | Array of `erow` (each holds `size` + `chars`) |
| `orig_termios` | Saved terminal settings for restore on exit |

---

## Key Design Decisions

- **Single `write()` per frame** via `abuf` prevents flickering — all escape sequences and content are buffered first.
- **`E.rowoff` as scroll state** keeps rendering simple: every draw just adds `rowoff` to the screen row index to get the file row.
- **`atexit(disableRawMode)`** ensures the terminal is always restored even if the program crashes via `die()`.
- **VT100 escape sequences** are used for all cursor/screen control — no ncurses dependency.

---

## Version

`0.0.1`
