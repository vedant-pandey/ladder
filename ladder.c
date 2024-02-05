#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/termios.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void disableRawMode(void) {
    // Reset to original flags for current terminal session
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Disable canonical mode(aka Cooked Mode) in terminal
void enableRawMode(void) {
    // Gets the attributes of running terminal in raw variable
    tcgetattr(STDIN_FILENO, &orig_termios);
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

    // Sets the terminal attributes from variable raw
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(void) {
    enableRawMode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
    }
    return 0;
}
