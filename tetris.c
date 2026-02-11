// Simple console Tetris (Windows) - minimal implementation
// Build: cl /W3 /O2 tetris.c or gcc tetris.c -o tetris
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <windows.h>
#include <stdint.h>

#define WIDTH 10
#define HEIGHT 20

static int board[HEIGHT][WIDTH];
typedef struct {
    uint16_t mask[4]; /* 4 rotations, 4x4 bitmask (bit index = y*4 + x) */
    int color;
} Tetromino;

/* Bit helper macro: (x,y) -> bit (y*4 + x) */
#define BIT(x,y) (1u << ((y) * 4 + (x)))

/* pieces: masks computed from original block coordinates */
static Tetromino pieces[7] = {
    /* I */
    { { 0x00F0 /* #### at row 1 */,
        0x4444 /* vertical at col 2 */,
        0x0F00 /* #### at row 2 */,
        0x2222 /* vertical at col 1 */ }, 1 },
    /* O */
    { { 0x0063, 0x0063, 0x0063, 0x0063 }, 2 },
    /* T */
    { { 0x0072, 0x0262, 0x0270, 0x0232 }, 3 },
    /* S */
    { { 0x0036, 0x0462, 0x0360, 0x0231 }, 4 },
    /* Z */
    { { 0x0063, 0x0264, 0x0630, 0x0132 }, 5 },
    /* J */
    { { 0x0071, 0x0226, 0x0470, 0x0322 }, 6 },
    /* L */
    { { 0x0074, 0x062A, 0x0170, 0x0223 }, 7 },
};

static int cur_piece, cur_rot;
static int cur_x, cur_y;
static int score;
static int level;
static int lines_total;
static int next_piece = -1;
static int hold_piece = -1;
static int hold_used;
static int speed_ms = 600;
static int game_over;

static void sleep_ms(int ms) {
    Sleep(ms);
}

static void clear_screen(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = {0,0};
    SetConsoleCursorPosition(hOut, pos);
}

/* Check whether the given piece (with rotation) fits at px,py on board */
static int fits_piece(int piece, int px, int py, int rot) {
    uint16_t m = pieces[piece].mask[rot & 3];
    for (int b = 0; b < 16; b++) {
        if ((m >> b) & 1u) {
            int bx = b % 4;
            int by = b / 4;
            int x = bx + px;
            int y = by + py;
            if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 0;
            if (board[y][x]) return 0;
        }
    }
    return 1;
}

static void update_speed(void) {
    int ms = 600 - (level - 1) * 25;
    if (ms < 60) ms = 60;
    speed_ms = ms;
}

static void set_current_piece(int piece) {
    cur_piece = piece;
    cur_rot = 0;
    cur_x = 3;
    cur_y = 0;
    if (!fits_piece(cur_piece, cur_x, cur_y, cur_rot)) game_over = 1;
}

static void spawn_piece(void) {
    if (next_piece < 0) next_piece = rand() % 7;
    set_current_piece(next_piece);
    next_piece = rand() % 7;
    hold_used = 0;
}

/* Lock blocks by testing mask bits */
static void lock_piece(void) {
    uint16_t m = pieces[cur_piece].mask[cur_rot & 3];
    for (int b = 0; b < 16; b++) {
        if ((m >> b) & 1u) {
            int bx = b % 4;
            int by = b / 4;
            int x = bx + cur_x;
            int y = by + cur_y;
            if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH)
                board[y][x] = pieces[cur_piece].color;
        }
    }
}

static int clear_lines(void) {
    int y, x, full, cleared = 0;
    for (y = HEIGHT - 1; y >= 0; y--) {
        full = 1;
        for (x = 0; x < WIDTH; x++) {
            if (board[y][x] == 0) { full = 0; break; }
        }
        if (full) {
            int yy;
            for (yy = y; yy > 0; yy--) {
                for (x = 0; x < WIDTH; x++) board[yy][x] = board[yy-1][x];
            }
            for (x = 0; x < WIDTH; x++) board[0][x] = 0;
            cleared++;
            y++;
        }
    }
    return cleared;
}

/* Mini preview cell test using rotation 0 mask */
static int mini_cell(int piece, int mx, int my) {
    if (piece < 0) return 0;
    if (mx < 0 || mx > 3 || my < 0 || my > 3) return 0;
    uint16_t m = pieces[piece].mask[0];
    int b = my * 4 + mx;
    return ((m >> b) & 1u) ? 1 : 0;
}

/* draw()를 대체: 한 프레임을 메모리 버퍼에 조립해 한 번에 출력하도록 변경했습니다.
   이유: putchar/printf를 반복 호출하면 프레임당 시스템 호출이 많아져
   출력이 느려지고 화면이 매끄럽지 않게 보입니다. */
static void draw(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    /* 좌측 보드(WIDTH + 2 경계) + 우측 패널 여유 */
    int cols = WIDTH + 14;
    int rows = HEIGHT + 3; /* score + board + separator + controls fits within */

    /* 버퍼 할당 (WriteConsoleOutputA용) */
    CHAR_INFO *buf = (CHAR_INFO*)malloc(sizeof(CHAR_INFO) * cols * rows);
    if (!buf) return;

    /* 초기화 (공백, 기본 색상) */
    for (int i = 0; i < cols * rows; i++) {
        buf[i].Char.AsciiChar = ' ';
        buf[i].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }

    /* 0행: Score, Level, Lines */
    char header[256];
    int hn = snprintf(header, sizeof(header), "Score: %d  Level: %d  Lines: %d", score, level, lines_total);
    if (hn < 0) hn = 0;
    for (int c = 0; c < hn && c < cols; c++) buf[c].Char.AsciiChar = header[c];

    /* 보드 및 우측 패널 구성 */
    for (int y = 0; y < HEIGHT; y++) {
        int row = 1 + y; /* 버퍼의 실제 행(0은 header) */
        int base = row * cols;

        /* 좌측 경계 '|' */
        buf[base + 0].Char.AsciiChar = '|';

        /* 보드 셀 */
        for (int x = 0; x < WIDTH; x++) {
            int cell = board[y][x];
            /* 현재 조종중인 블록을 덧붙임 (mask 기반) */
            uint16_t m = pieces[cur_piece].mask[cur_rot & 3];
            for (int b = 0; b < 16; b++) {
                if ((m >> b) & 1u) {
                    int bx = b % 4;
                    int by = b / 4;
                    int bx_abs = bx + cur_x;
                    int by_abs = by + cur_y;
                    if (bx_abs == x && by_abs == y) {
                        cell = pieces[cur_piece].color;
                        break;
                    }
                }
            }
            buf[base + 1 + x].Char.AsciiChar = cell ? '#' : ' ';
        }

        /* 우측 경계 '|' */
        buf[base + 1 + WIDTH].Char.AsciiChar = '|';

        /* 우측 패널 (Next / Hold) - 원본 draw와 동일한 라인 배치에 맞춤 */
        int side = 1 + WIDTH + 2; /* 우측 패널 시작 열 */
        if (y == 0) {
            const char *s = "  Next:";
            for (int k = 0; s[k] && side + k < cols; k++) buf[base + side + k].Char.AsciiChar = s[k];
        } else if (y >= 1 && y <= 4) {
            for (int mx = 0; mx < 4 && side + mx < cols; mx++) {
                buf[base + side + mx].Char.AsciiChar = mini_cell(next_piece, mx, y - 1) ? '#' : ' ';
            }
        } else if (y == 6) {
            const char *s = "  Hold:";
            for (int k = 0; s[k] && side + k < cols; k++) buf[base + side + k].Char.AsciiChar = s[k];
        } else if (y >= 7 && y <= 10) {
            for (int mx = 0; mx < 4 && side + mx < cols; mx++) {
                buf[base + side + mx].Char.AsciiChar = mini_cell(hold_piece, mx, y - 7) ? '#' : ' ';
            }
        }
    }

    /* 구분선 (보드 아래) */
    int sep_row = 1 + HEIGHT;
    if (sep_row < rows) {
        for (int x = 0; x < WIDTH + 2 && x < cols; x++) {
            buf[sep_row * cols + x].Char.AsciiChar = '-';
        }
    }

    /* Controls 라인 */
    const char *ctrls = "Controls: Left/Right arrows move, W rotate, S soft drop, Space hard drop, C hold, Q quit";
    int ctrl_row = sep_row + 1;
    if (ctrl_row < rows) {
        for (int k = 0; ctrls[k] && k < cols; k++) buf[ctrl_row * cols + k].Char.AsciiChar = ctrls[k];
    }

    /* 출력 영역 및 버퍼 사이즈 설정 */
    COORD bufSize = { (SHORT)cols, (SHORT)rows };
    COORD bufCoord = { 0, 0 };
    SMALL_RECT writeRegion = { 0, 0, (SHORT)(cols - 1), (SHORT)(rows - 1) };

    /* 콘솔 버퍼 크기 설정 (스크롤/깜빡임 방지에 도움) */
    COORD newSize = { (SHORT)cols, (SHORT)rows };
    SetConsoleScreenBufferSize(hOut, newSize);

    /* 한 번에 출력 */
    WriteConsoleOutputA(hOut, buf, bufSize, bufCoord, &writeRegion);

    free(buf);
}

int main(void) {
    const int line_scores[5] = {0, 100, 300, 500, 800};
    srand((unsigned)time(NULL));
    level = 1;
    lines_total = 0;
    update_speed();
    spawn_piece();

    while (!game_over) {
        int skip_fall = 0;
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 0 || ch == 0xE0) {
                /* extended key (arrow keys, function keys) */
                int ch2 = _getch();
                if (ch2 == 75) { /* left arrow */
                    if (fits_piece(cur_piece, cur_x - 1, cur_y, cur_rot)) cur_x--;
                } else if (ch2 == 77) { /* right arrow */
                    if (fits_piece(cur_piece, cur_x + 1, cur_y, cur_rot)) cur_x++;
                }
            } else {
                if (ch == 'q' || ch == 'Q') break;
                if (ch == 'w' || ch == 'W') {
                     int nr = (cur_rot + 1) % 4;
                     if (fits_piece(cur_piece, cur_x, cur_y, nr)) cur_rot = nr;
                 } else if (ch == 's' || ch == 'S') {
                     if (fits_piece(cur_piece, cur_x, cur_y + 1, cur_rot)) {
                        cur_y++;
                        score += 1;
                     }
                 } else if (ch == ' ') {
                    int drop = 0;
                    while (fits_piece(cur_piece, cur_x, cur_y + 1, cur_rot)) {
                        cur_y++;
                        drop++;
                    }
                    score += drop * 2;
                    lock_piece();
                    {
                        int cleared = clear_lines();
                        if (cleared > 0) {
                            score += line_scores[cleared] * level;
                            lines_total += cleared;
                            level = lines_total / 10 + 1;
                            update_speed();
                        }
                    }
                    spawn_piece();
                    skip_fall = 1;
                } else if (ch == 'c' || ch == 'C') {
                    if (!hold_used) {
                        if (hold_piece < 0) {
                            hold_piece = cur_piece;
                            spawn_piece();
                        } else {
                            int temp = hold_piece;
                            hold_piece = cur_piece;
                            set_current_piece(temp);
                        }
                        hold_used = 1;
                        skip_fall = 1;
                    }
                }
            }
        }

        if (!skip_fall) {
            if (fits_piece(cur_piece, cur_x, cur_y + 1, cur_rot)) {
                cur_y++;
            } else {
                lock_piece();
                {
                    int cleared = clear_lines();
                    if (cleared > 0) {
                        score += line_scores[cleared] * level;
                        lines_total += cleared;
                        level = lines_total / 10 + 1;
                        update_speed();
                    }
                }
                spawn_piece();
            }
        }

        draw();
        sleep_ms(speed_ms);
    }

    printf("Game Over! Final score: %d\n", score);
    return 0;
}
