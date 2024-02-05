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
    // Disable each key to be printed in the terminal as a  local flag
    raw.c_lflag &= ~(ECHO | ICANON);

    // Sets the terminal attributes from variable raw
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(void) {
    enableRawMode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}
