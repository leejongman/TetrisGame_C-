#include "tetris.h"
#include <stdlib.h>

void safe_release(IUnknown* p) {
    if (p) p->Release();
}

uint16_t rotate_mask_once(uint16_t m) {
    uint16_t out = 0;
    for (int b = 0; b < 16; b++) {
        if ((m >> b) & 1u) {
            int x = b % 4;
            int y = b / 4;
            int nx = y;
            int ny = 3 - x;
            int nb = ny * 4 + nx;
            out |= (1u << nb);
        }
    }
    return out;
}

uint16_t rotate_mask(uint16_t m, int rot) {
    rot &= 3;
    uint16_t r = m;
    for (int i = 0; i < rot; i++) r = rotate_mask_once(r);
    return r;
}

uint16_t get_mask(int piece, int rot) {
    if (piece < 0 || piece >= 7) return 0;
    uint16_t base = pieces[piece].mask[0];
    return rotate_mask(base, rot & 3);
}

int fits_piece(int piece, int px, int py, int rot) {
    uint16_t m = get_mask(piece, rot);
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

void update_speed(void) {
    int ms = 600 - (level - 1) * 25;
    if (ms < 60) ms = 60;
    speed_ms = ms;
}

void set_current_piece(int piece) {
    cur_piece = piece;
    cur_rot = 0;
    cur_x = 3;
    cur_y = 0;
    if (!fits_piece(cur_piece, cur_x, cur_y, cur_rot)) game_over = 1;
}

void spawn_piece(void) {
    if (next_piece < 0) next_piece = rand() % 7;
    set_current_piece(next_piece);
    next_piece = rand() % 7;
    hold_used = 0;
}

void lock_piece(void) {
    uint16_t m = get_mask(cur_piece, cur_rot);
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

int clear_lines(void) {
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

void game_tick(HWND hwnd) {
    if (fits_piece(cur_piece, cur_x, cur_y + 1, cur_rot)) {
        cur_y++;
    } else {
        lock_piece();
        int cleared = clear_lines();
        if (cleared > 0) {
            const int line_scores[5] = {0, 100, 300, 500, 800};
            score += line_scores[cleared] * level;
            lines_total += cleared;
            level = lines_total / 10 + 1;
            update_speed();
            reset_timer(hwnd);
        }
        spawn_piece();
    }
    InvalidateRect(hwnd, NULL, FALSE);
}