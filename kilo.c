#include <unistd.h>
#include<termios.h>
#include<stdlib.h>
#include <ctype.h>
#include <stdio.h>

struct termios orig_termios;
//Raw mode is a terminal mode in which input and output are handled without any processing 
//or modification by the terminal. In raw mode:
// A mask is abit pattern used to modify specific bits in a variable without affecting
// the other bits 
void disableRawMode(){
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios);
}


void enableRawMode(){
    tcgetattr(STDIN_FILENO, &orig_termios);//used to get paramters associated witht the terminal
    //reads current attribute into a struct 
    atexit(disableRawMode);
    // atexit() comes from <stdlib.h>. We use it to register 
    // our disableRawMode() function to be called automatically when the program exits, whether it exits by returning from main(), or by calling the exit() function. This way we can ensure we’ll leave the terminal attributes 
    // the way we found them when our program exits.
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICRNL|IXON);//TURNS off ctrls/q that are used for soft control to stop data being transmitted 
    //ICRNL turns of carriage return done by the terminal 
    raw.c_oflag&=~(OPOST);
    raw.c_lflag &=~(ECHO | ICANON | ISIG |IEXTEN);
    //ISIG TURNS OF CTRLZ AND CTRLC THATI SIGIINT AND SIGSTP
    //this & is not for address but a bitwise operation here 
    // anding the not of ECHO flag 
    // ECHO is a bitflag, defined as 
    // 00000000000000000000000000001000 in binary. 
    // We use the bitwise-NOT operator (~) on this value t
    // o get 11111111111111111111111111110111. 
    // We then bitwise-AND this value with the flags field, 
    // which forces the fourth bit in the flags 
    // field to become 0, and causes every other bit 
    // to retain its current value. 
    //Flipping bits like this is common in C.
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw);
    // the TCSAFLUSH argument specifies when to apply the change: in this case, it waits for all pending output to be written to the terminal, 
    // and also discards any input that hasn’t been read.
}
///r/n - moves the cursor to the beginning of the line along with moving the cursr to the next line 
int main() {
  enableRawMode();
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q'){
    //iscntrl tests whether a char is a control character,i.e. non printable characters 
    if(iscntrl(c)){
        printf("%d\n", c);
    }else{
        printf("%d ('%c')\r\n", c, c);
    }
  };
  return 0;
}