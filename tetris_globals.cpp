#include "tetris.h"
/* Game board and state */
int board[HEIGHT][WIDTH] = {0};

int cur_piece = 0, cur_rot = 0;
int cur_x = 3, cur_y = 0;
int score = 0;
int level = 1;
int lines_total = 0;
int next_piece = -1;
int hold_piece = -1;
int hold_used = 0;
int speed_ms = 600;
int game_over = 0;
int animation_frame = 0;

/* Direct2D objects */
ID2D1Factory* d2d_factory = NULL;
ID2D1HwndRenderTarget* render_target = NULL;
ID2D1Bitmap* background_bitmap = NULL;
ID2D1SolidColorBrush* brushes[8] = { NULL };
ID2D1SolidColorBrush* brush_border = NULL;
ID2D1SolidColorBrush* brush_bg = NULL;
ID2D1SolidColorBrush* brush_label = NULL;
ID2D1SolidColorBrush* brush_label_score = NULL;
ID2D1SolidColorBrush* brush_label_lines = NULL;
ID2D1SolidColorBrush* brush_label_level = NULL;
ID2D1SolidColorBrush* brush_value = NULL;

/* DirectWrite objects */
IDWriteFactory* dwrite_factory = NULL;
IDWriteTextFormat* text_format = NULL;

/* Layout constants */
const int cell_size = 28;  /* reduced from 42 */
const int cell_gap = 2;    /* reduced from 3 */
const int board_left = 24;  /* 16 * 1.5 */
const int board_top = 72;   /* 48 * 1.5 */
const int side_panel_left_offset = WIDTH * (cell_size + cell_gap) + 72;  /* Adjusted for larger board */

/* Tetromino pieces */
Tetromino pieces[7] = {
    /* I */
    { { 0x00F0, 0, 0, 0 }, 1 },
    /* O */
    { { 0x0066, 0, 0, 0 }, 2 },
    /* T */
    { { 0x0072, 0, 0, 0 }, 3 },
    /* S */
    { { 0x0036, 0, 0, 0 }, 4 },
    /* Z */
    { { 0x0063, 0, 0, 0 }, 5 },
    /* J */
    { { 0x0071, 0, 0, 0 }, 6 },
    /* L */
    { { 0x0074, 0, 0, 0 }, 7 },
};