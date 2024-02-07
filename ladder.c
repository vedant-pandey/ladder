/*** includes ***/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define LADDER_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)
#define READ_FILE(buf, buf_len) (read(STDIN_FILENO, buf, buf_len))
#define WRITE_FILE(buf, buf_len) (write(STDOUT_FILENO, buf, buf_len))

#define ES27_C '\x1b'
#define CRNL "\r\n"
#define ES27_S "\x1b"

enum EditorKey {
    MOVE_UP = 1000,
    MOVE_DOWN,
    MOVE_RIGHT,
    MOVE_LEFT,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY,
};

/*** data ***/

struct editorConfig {
    int cx, cy;
    int screen_rows;
    int screen_cols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void resetCursor(void) {
    WRITE_FILE(ES27_S"[2J", 4);
    WRITE_FILE(ES27_S"[H", 3);
}

void die(const char *s) {
    resetCursor();

    perror(s);
    exit(1);
}

void disableRawMode(void) {
    // Reset to original flags for current terminal session
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

// Disable canonical mode(aka Cooked Mode) in terminal
void enableRawMode(void) {
    // Gets the attributes of running terminal in raw variable
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;

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

int editorReadKey(void) {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == ES27_C) {
        char seq[3];

        if (READ_FILE(&seq[0], 1) != 1) return ES27_C;
        if (READ_FILE(&seq[1], 1) != 1) return ES27_C;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (READ_FILE(&seq[2], 1) != 1) return ES27_C;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return MOVE_UP;
                    case 'B': return MOVE_DOWN;
                    case 'C': return MOVE_RIGHT;
                    case 'D': return MOVE_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return ES27_C;
    }
    
    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (WRITE_FILE(ES27_S"[6n", 4) != 4) return -1;

    while (i < sizeof(buf) -1) {
        if (READ_FILE(&buf[i], 1) != 1) break;
        // 'R' represents the end of byte sequence for cursor report escape sequence
        if (buf[i] == 'R') break;
        ++i;
    }
    buf[i] = '\0';

    if (buf[0] != ES27_C || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // Goes to bottom right of the screen, 999 represents arbitrarily large value
        if (WRITE_FILE(ES27_S"[999C"ES27_S"[999B", 12) != 12) return -1;
        // Get the current cursor position to record the size of the window
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** append buffer ***/

// Implementation of dynamic strings
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}
typedef struct abuf Abuf;

void abAppend(Abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(Abuf *ab) {
    free(ab->b);
}

/*** input ***/

void editorMoveCursor(int key) {
    switch (key) {
        case MOVE_LEFT:
            if (E.cx > 0) E.cx--;
            break;
        case MOVE_UP:
            if (E.cy > 0) E.cy--;
            break;
        case MOVE_DOWN:
            if (E.cy < E.screen_cols - 1) E.cy++;
            break;
        case MOVE_RIGHT:
            if (E.cx < E.screen_rows - 1) E.cx++;
            break;
    }
}

void editorProcessKeypress(void) {
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            resetCursor();
            exit(0);
            break;

        case HOME_KEY:
            E.cx = 0;
            break;

        case END_KEY:
            E.cx = E.screen_cols - 1;
            break;

        case DEL_KEY:
            // Do nothing for now
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            // This scope is required to be declared because C doesn't allow
            // direct variable declaration in switch
            {
                int times = E.screen_rows;
                while (times--) editorMoveCursor(c == PAGE_UP ? MOVE_UP : MOVE_DOWN);
            }
            break;

        case MOVE_UP:
        case MOVE_DOWN:
        case MOVE_RIGHT:
        case MOVE_LEFT:
            editorMoveCursor(c);
            break;
    }
}

/*** output ***/

void drawWelcomeMessage(Abuf *ab) {
    char welcome[80];
    int welcome_len = snprintf(welcome, sizeof(welcome), "Ladder editor -- version %s", LADDER_VERSION);
    if (welcome_len > E.screen_cols) welcome_len = E.screen_cols;
    int padding = (E.screen_cols - welcome_len) / 2;
    if (padding) {
        abAppend(ab, "~", 1);
        --padding;
    }
    while (padding--) abAppend(ab, " ", 1);
    abAppend(ab, welcome, welcome_len);

}

void editorDrawRows(Abuf *ab) {
    int y;
    for (y = 0; y < E.screen_rows; ++y) {
        if (y == E.screen_rows / 3) {
            drawWelcomeMessage(ab);
        } else {
            abAppend(ab, "~", 1);
        }

        // Clear one line at a time
        abAppend(ab, ES27_S"[K", 3);

        // printing newline for last row makes terminal scroll up
        // this ensures last line doesn't have CRNL in it 
        if (y < E.screen_rows - 1) {
            abAppend(ab, CRNL, 2);
        }
    }
}

void editorRefreshScreen(void) {
    Abuf ab = ABUF_INIT;
    // 4 describes to write 4 bytes in the terminal
    // \x1b initiates escape sequence and is 27 in decimal
    // [2J are other 3 bytes which are the escape sequence for clear screen
    // Ref https://vt100.net/docs/vt100-ug/chapter3.html#ED

    // Escape sequence to hide cursor which might not be supported in older terminals
    abAppend(&ab, ES27_S"[?25l", 6);
    abAppend(&ab, ES27_S"[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), ES27_S"[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, 6);

    abAppend(&ab, ES27_S"[?25h", 6);

    WRITE_FILE(ab.b, ab.len);
    abFree(&ab);
}

/*** init ***/

void initEditor(void) {
    E.cx = 0;
    E.cy = 0;
    
    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) die("getWindowSize");
}

int main(void) {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
