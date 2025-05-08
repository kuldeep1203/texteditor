
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <termios.h>
#include<string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include<sys/types.h>


/**file i/o */
void editorOpen(char *filename){

    FILE *fp = fopen(filename,"r");
    if(!fp) die("fopen");



    char *line = "Hello World";
    ssize_t linelen;
    ssize_t linecap = 0;
    linelen = getline(&line,&linecap,fp);
    if(linelen!=-1){
        while(linelen > 0 && (line[linelen-1]=='\n'|| line[linelen - 1]=='\r')) linelen--;
        E.row.size = linelen;
        E.row.chars = malloc(linelen+1);
        memcpy(E.row.chars,line,linelen);
        E.row.chars[linelen]='\0';
        E.numrows =1 ;
    }
    free(line);
    fclose(fp);
}


/*** Defines ***/
#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f) // Macro to interpret Ctrl + key input
#define ABUF_INIT {NULL,0}

/**append buffer */
struct abuf{
  char *b;
  int len;
};

enum editorKey{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP ,
    ARROW_DOWN ,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY

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


typedef struct erow {
    int size;
    char *chars;
}erow;




struct editorConfig {
    int cx,cy;//coordiantes of cursor 
    int screenrows; // Number of rows in the terminal window
    int screencols; // Number of columns in the terminal window
    struct termios orig_termios; // Stores the original terminal settings
    int numrows;
    erow row;
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
int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    if(c=='\x1b'){//for arrow keys they translate to a squence that strats with '\x1b' followed by [A/B/C/D 
        char seq[3];
        
        if(read(STDIN_FILENO,&seq[0],1)!=1) return '\x1b';
        if(read(STDIN_FILENO,&seq[1],1)!=1) return '\x1b';

        if(seq[0]=='['){
            if(seq[1]>='0' && seq[1]<='9'){
                if(read(STDIN_FILENO,&seq[2],1)!=1) return '\x1b';
                if(seq[2]=='~'){
                    switch(seq[1]){
                        case '1':return HOME_KEY;
                        case  '2': return END_KEY;
                        case '5':return PAGE_UP;
                        case '6':return  PAGE_DOWN;
                        case '7':return HOME_KEY;
                        case '8':return END_KEY;   
                        case '3':return DEL_KEY;               
                    }
                }
            }

            else{
                  switch(seq[1]){
                    case 'A' : return ARROW_UP;
                    case 'B' :return ARROW_DOWN;
                    case 'C' :return ARROW_RIGHT;
                    case  'D':return ARROW_LEFT;     
                    case 'H':return HOME_KEY;
                    case 'F':return END_KEY;    
                }
            }
          
        }else if(seq[0]=='0'){
            switch(seq[1]){
                case 'H' : return HOME_KEY;
                case 'F' : return END_KEY;
            }
        }
        return '\x1b';
    }else  return c;
 
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
        if(y>=E.numrows){
            if(y==E.screenrows/3){
                char welcome[80];
                int welcomelen = snprintf(welcome,sizeof(welcome),"Kilo editor --version%s",KILO_VERSION);
                
                if(welcomelen>E.screencols) welcomelen  = E.screencols; //default the size to screen size incase its smaller then welcome
                
                int padding = (E.screencols-welcomelen)/2;
                if(padding){
                    abAppend(ab,"~",1);
                    padding--;
                }
            while(padding--) abAppend(ab,"",1);
            abAppend(ab,welcome,welcomelen);
            }else{
            abAppend(ab,"~",1);
            }   
        }
        else{
            int len  = E.row.size;
            if(len>E.screencols) len  = E.screencols;
            abAppend(ab,E.row.chars,len);
        }



        abAppend(ab,"~",1);

        abAppend(ab,"\x1b[K",3);
        if(y<E.screenrows-1){
          write(STDOUT_FILENO,"\r\n",2);
        }
    }
}

// We use escape sequences to tell the terminal to hide and show the cursor. 
// The h and l commands (Set Mode, Reset Mode) are used to turn on and turn off various terminal features or “modes”. 
// The VT100 User Guide just linked to doesn’t document argument ?25 which we use above. 
// It appears the cursor hiding/showing feature appeared in later VT models. 

// Refreshes the screen by clearing it and redrawing the rows
void editorRefreshScreen() {
    struct abuf ab  = ABUF_INIT;

    abAppend(&ab,"\x1b[?25l",6);//hide the cursor before refereshing the screen

    
    abAppend(&ab,"\x1b[H",3);// Moves the cursor to the top-left corner

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cy+1,E.cx+1);
    abAppend(&ab,buf,strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** Input ***/


void editorMoveCursor(int key){
    switch(key){
        case ARROW_LEFT:
            if(E.cx!=0){
                 E.cx--;
            }
            break;
        case ARROW_RIGHT :
            if(E.cx!=E.screencols-1){
                E.cx++;
            }
            break;
        case ARROW_UP:
            if(E.cy!=0){
                 E.cy--;
            }
            break;
        case ARROW_DOWN:
            if(E.cy!=E.screenrows-1){
                 E.cy++;
            }
            break;
    }
}



// Processes user keypresses and handles them accordingly
void editorProcessKeypress() {
    int c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'): // If Ctrl+Q is pressed, quit the program
            write(STDOUT_FILENO, "\x1b[2J", 4); // Clears the screen
            write(STDOUT_FILENO, "\x1b[H", 3);  // Moves the cursor to the top-left corner
            exit(0);
            break;

        case HOME_KEY:
            E.cx=0;
            break;
        case END_KEY:
            E.cx = E.screencols -1 ;
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screenrows;
                while(times--)
                    editorMoveCursor(c==PAGE_UP? ARROW_UP : ARROW_DOWN);
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

/*** Initialization ***/

// Initializes the editor by retrieving the window size
void initEditor() {

    E.cy = 0 ;
    E.cx = 0;
    E.numrows=0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

/*** Main ***/

int main(int argc ,char *argv[]) {
    enableRawMode(); // Enable raw mode for the terminal
    initEditor();    // Initialize the editor configuration

    if(argc>=2){
        editorOpen(argv[1]);
    }
    while (1) {
        editorRefreshScreen();   // Refresh the screen on each iteration
        editorProcessKeypress(); // Wait for and handle user input
    }

    return 0;
}
