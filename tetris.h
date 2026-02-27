#ifndef TETRIS_H
#define TETRIS_H

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <stdint.h>

#define WIDTH 10
#define HEIGHT 30

/* Tetromino structure */
typedef struct {
    uint16_t mask[4];
    int color;
} Tetromino;

/* Constants */
extern const int cell_size;
extern const int cell_gap;
extern const int board_left;
extern const int board_top;
extern const int side_panel_left_offset;

/* Game state */
extern int board[HEIGHT][WIDTH];
extern int cur_piece, cur_rot;
extern int cur_x, cur_y;
extern int score, level, lines_total;
extern int next_piece, hold_piece, hold_used;
extern int speed_ms, game_over;
extern int animation_frame;

/* D2D objects */
extern ID2D1Factory* d2d_factory;
extern ID2D1HwndRenderTarget* render_target;
extern ID2D1Bitmap* background_bitmap;
extern ID2D1SolidColorBrush* brushes[8];
extern ID2D1SolidColorBrush* brush_border;
extern ID2D1SolidColorBrush* brush_bg;
extern ID2D1SolidColorBrush* brush_label;
extern ID2D1SolidColorBrush* brush_label_score;
extern ID2D1SolidColorBrush* brush_label_level;
extern ID2D1SolidColorBrush* brush_label_lines;
extern ID2D1SolidColorBrush* brush_value;

/* DirectWrite objects */
extern IDWriteFactory* dwrite_factory;
extern IDWriteTextFormat* text_format;

/* Tetromino definitions */
extern Tetromino pieces[7];

/* Game functions */
void update_speed(void);
int fits_piece(int piece, int px, int py, int rot);
void lock_piece(void);
int clear_lines(void);
void spawn_piece(void);
void set_current_piece(int piece);
void game_tick(HWND hwnd);

/* Graphics functions */
void create_d2d_resources(HWND hwnd);
void discard_d2d_resources(void);
void on_paint(HWND hwnd);
ID2D1Bitmap* load_image_from_file(ID2D1RenderTarget* rt, const wchar_t* filename);

/* Utility functions */
void safe_release(IUnknown* p);
uint16_t rotate_mask_once(uint16_t m);
uint16_t rotate_mask(uint16_t m, int rot);
uint16_t get_mask(int piece, int rot);
void draw_cell(ID2D1RenderTarget* rt, int bx, int by, ID2D1SolidColorBrush* brush);
void reset_timer(HWND hwnd);

#endif /* TETRIS_H */