// Simple console Tetris (Windows) - minimal implementation
// Build: cl /W3 /O2 tetris.c or gcc tetris.c -o tetris
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <windows.h>

#define WIDTH 10
#define HEIGHT 20

static int board[HEIGHT][WIDTH];

typedef struct { int x, y; } Point;

typedef struct {
    Point blocks[4][4]; // 4 rotations, 4 blocks each
    int color;
} Tetromino;

static Tetromino pieces[7] = {
    // I
    { { { {0,1},{1,1},{2,1},{3,1} }, { {2,0},{2,1},{2,2},{2,3} }, { {0,2},{1,2},{2,2},{3,2} }, { {1,0},{1,1},{1,2},{1,3} } }, 1 },
    // O
    { { { {1,0},{2,0},{1,1},{2,1} }, { {1,0},{2,0},{1,1},{2,1} }, { {1,0},{2,0},{1,1},{2,1} }, { {1,0},{2,0},{1,1},{2,1} } }, 2 },
    // T
    { { { {1,0},{0,1},{1,1},{2,1} }, { {1,0},{1,1},{2,1},{1,2} }, { {0,1},{1,1},{2,1},{1,2} }, { {1,0},{0,1},{1,1},{1,2} } }, 3 },
    // S
    { { { {1,0},{2,0},{0,1},{1,1} }, { {1,0},{1,1},{2,1},{2,2} }, { {1,1},{2,1},{0,2},{1,2} }, { {0,0},{0,1},{1,1},{1,2} } }, 4 },
    // Z
    { { { {0,0},{1,0},{1,1},{2,1} }, { {2,0},{1,1},{2,1},{1,2} }, { {0,1},{1,1},{1,2},{2,2} }, { {1,0},{0,1},{1,1},{0,2} } }, 5 },
    // J
    { { { {0,0},{0,1},{1,1},{2,1} }, { {1,0},{2,0},{1,1},{1,2} }, { {0,1},{1,1},{2,1},{2,2} }, { {1,0},{1,1},{0,2},{1,2} } }, 6 },
    // L
    { { { {2,0},{0,1},{1,1},{2,1} }, { {1,0},{1,1},{1,2},{2,2} }, { {0,1},{1,1},{2,1},{0,2} }, { {0,0},{1,0},{1,1},{1,2} } }, 7 },
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

static int fits_piece(int piece, int px, int py, int rot) {
    int i;
    for (i = 0; i < 4; i++) {
        int x = pieces[piece].blocks[rot][i].x + px;
        int y = pieces[piece].blocks[rot][i].y + py;
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 0;
        if (board[y][x]) return 0;
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

static void lock_piece(void) {
    int i;
    for (i = 0; i < 4; i++) {
        int x = pieces[cur_piece].blocks[cur_rot][i].x + cur_x;
        int y = pieces[cur_piece].blocks[cur_rot][i].y + cur_y;
        if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH)
            board[y][x] = pieces[cur_piece].color;
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

static int mini_cell(int piece, int mx, int my) {
    int i;
    if (piece < 0) return 0;
    for (i = 0; i < 4; i++) {
        int x = pieces[piece].blocks[0][i].x;
        int y = pieces[piece].blocks[0][i].y;
        if (x == mx && y == my) return 1;
    }
    return 0;
}

static void draw(void) {
    int x, y, mx;
    clear_screen();
    printf("Score: %d  Level: %d  Lines: %d\n", score, level, lines_total);

    for (y = 0; y < HEIGHT; y++) {
        printf("|");
        for (x = 0; x < WIDTH; x++) {
            int cell = board[y][x];
            int i;
            for (i = 0; i < 4; i++) {
                int bx = pieces[cur_piece].blocks[cur_rot][i].x + cur_x;
                int by = pieces[cur_piece].blocks[cur_rot][i].y + cur_y;
                if (bx == x && by == y) cell = pieces[cur_piece].color;
            }
            putchar(cell ? '#' : ' ');
        }
        printf("|");
        if (y == 0) {
            printf("  Next:");
        } else if (y >= 1 && y <= 4) {
            printf("  ");
            for (mx = 0; mx < 4; mx++) putchar(mini_cell(next_piece, mx, y - 1) ? '#' : ' ');
        } else if (y == 6) {
            printf("  Hold:");
        } else if (y >= 7 && y <= 10) {
            printf("  ");
            for (mx = 0; mx < 4; mx++) putchar(mini_cell(hold_piece, mx, y - 7) ? '#' : ' ');
        }
        printf("\n");
    }
    for (x = 0; x < WIDTH + 2; x++) putchar('-');
    putchar('\n');
    printf("Controls: Left/Right arrows move, W rotate, S soft drop, Space hard drop, C hold, Q quit\n");
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
