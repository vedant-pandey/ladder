#include <termios.h>
#include <unistd.h>

// Disable canonical mode(aka Cooked Mode) in terminal
void enableRawMode(void) {
    struct termios raw;

    // Gets the attributes of running terminal in raw variable
    tcgetattr(STDIN_FILENO, &raw);

    // c_lflag is for local flags
    // ECHO is a feature that echoes any key pressed to the terminal
    // Disable each key to be printed in the terminal as a  local flag
    raw.c_lflag &= ~(ECHO);

    // Sets the terminal attributes from variable raw
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(void) {
    enableRawMode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}
