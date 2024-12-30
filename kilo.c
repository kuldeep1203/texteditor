#include <unistd.h>
#include <termios.h>
#include<string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>

/*** Defines ***/
#define CTRL_KEY(k) ((k) & 0x1f) // Macro to interpret Ctrl + key input
#define ABUF_INIT {NULL,0}

/**append buffer */
struct abuf{
  char *b;
  int len;
};

void abAppend(struct abuf *ab,const char *s,int len){
  char *new = realloc(ab->b,ab->len+len);

  if(new == NULL) return;
  memcpy(&new[ab->len],s,len);
  ab->b =new;
  ab->len+= len;
}

void abFree(struct abuf *ab){
  free(ab->b);
}


/*** Data ***/
struct editorConfig {
    int screenrows; // Number of rows in the terminal window
    int screencols; // Number of columns in the terminal window
    struct termios orig_termios; // Stores the original terminal settings
};

struct editorConfig E; // Global editor configuration

/*** Terminal ***/




// Cleans up terminal and exits program on error
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clears the entire screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // Moves the cursor to the top-left corner
    perror(s);
    exit(1);
}

// Restores the terminal to its original mode
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

// Configures terminal into raw mode
void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr"); // Saves current terminal settings

    atexit(disableRawMode); // Ensures terminal settings are restored on program exit

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Disables specific input flags
    raw.c_oflag &= ~(OPOST); // Disables output post-processing
    raw.c_cflag |= (CS8); // Sets character size to 8 bits per byte
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // Disables canonical mode, echo, and signals
    raw.c_cc[VMIN] = 0; // Minimum number of bytes for read()
    raw.c_cc[VTIME] = 1; // Timeout for read()

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

// Reads a single key press from the user
char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  while(i<sizeof(buf)-1){
    if(read(STDIN_FILENO,&buf[i],1)!=1)break;
    if(buf[i]=='R')break;
    i++;
  }
  buf[i]='\0';
  if(buf[0]!='\x1b'||buf[1]!='[')return -1;
  if(sscanf(&buf[2],"%d;%d",rows,cols)!=2) return -1;
  return 0;
}
// Retrieves the size of the terminal window
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)return -1;
        return getCursorPosition(rows,cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** Output ***/

// Draws tildes (~) on each row of the terminal
void editorDrawRows(struct abuf *ab) {
    for (int y = 0; y < E.screenrows; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
        if(y<E.screenrows-1){
          write(STDOUT_FILENO,"\r\n",2);
        }
    }
}

// Refreshes the screen by clearing it and redrawing the rows
void editorRefreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clears the screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // Moves the cursor to the top-left corner
    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);  // Resets the cursor position
}

/*** Input ***/

// Processes user keypresses and handles them accordingly
void editorProcessKeypress() {
    char c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'): // If Ctrl+Q is pressed, quit the program
            write(STDOUT_FILENO, "\x1b[2J", 4); // Clears the screen
            write(STDOUT_FILENO, "\x1b[H", 3);  // Moves the cursor to the top-left corner
            exit(0);
            break;
    }
}

/*** Initialization ***/

// Initializes the editor by retrieving the window size
void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

/*** Main ***/

int main() {
    enableRawMode(); // Enable raw mode for the terminal
    initEditor();    // Initialize the editor configuration

    while (1) {
        editorRefreshScreen();   // Refresh the screen on each iteration
        editorProcessKeypress(); // Wait for and handle user input
    }

    return 0;
}
