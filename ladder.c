/*** includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/termios.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode(void) {
    // Reset to original flags for current terminal session
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

// Disable canonical mode(aka Cooked Mode) in terminal
void enableRawMode(void) {
    // Gets the attributes of running terminal in raw variable
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = orig_termios;

    // c_lflag is for local flags
    // ECHO is a feature that echoes any key pressed to the terminal
    // ICANON is flag for canonical mode(input is read on pressing enter)
    // ISIG is flag for interrupt signals
    // IXON enables XON(<C-s> to pause data transmission and <C-q> resumes)
    // for certain terminals
    // IEXTEN is flag that let's you send special characters(eg <C-c>)
    // to terminal after pressing <C-v>
    // ICRNL is flag that enables echoing for new line and carriage return
    // OPOST flag sends \r\n when \n is pressed
    // Disable each key to be printed in the terminal as a  local flag
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    raw.c_cc[VMIN] = 0; // Minimum number of bytes of input needed before read returns
    raw.c_cc[VTIME] = 1; // Timeout of read until it returns(unit deciseconds)

    // Sets the terminal attributes from variable raw
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey(void) {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    return c;
}

/*** input ***/

void editorProcessKeypress(void) {
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            exit(0);
            break;
    }
}

/*** output ***/

void editoryRefreshScreen() {
    // 4 describes to write 4 bytes in the terminal
    // \x1b initiates escape sequence and is 27 in decimal
    // [2J are other 3 bytes which are the escape sequence for clear screen
    // Ref https://vt100.net/docs/vt100-ug/chapter3.html#ED
    write(STDOUT_FILENO, "\x1b[2J", 4);
}

/*** init ***/

int main(void) {
    enableRawMode();

    while (1) {
        editoryRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
