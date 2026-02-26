// tetris.cpp
// Direct2D GUI Tetris (Windows)
// Build in Visual Studio as C++ (or set file property: Compile as C++)
// Auto-link to d2d1.lib
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

#define WIDTH 10
#define HEIGHT 20

static int board[HEIGHT][WIDTH];

typedef struct {
    uint16_t mask[4]; /* store canonical mask in mask[0]; rotations computed at runtime */
    int color;
} Tetromino;

/* pieces: canonical masks in mask[0]; other entries retained for compatibility */
static Tetromino pieces[7] = {
    /* I */
    { { 0x00F0, 0,0,0 }, 1 },
    /* O */
    { { 0x0063, 0,0,0 }, 2 },
    /* T */
    { { 0x0072, 0,0,0 }, 3 },
    /* S */
    { { 0x0036, 0,0,0 }, 4 },
    /* Z */
    { { 0x0063, 0,0,0 }, 5 },
    /* J */
    { { 0x0071, 0,0,0 }, 6 },
    /* L */
    { { 0x0074, 0,0,0 }, 7 },
};

static int cur_piece = 0, cur_rot = 0;
static int cur_x = 3, cur_y = 0;
static int score = 0;
static int level = 1;
static int lines_total = 0;
static int next_piece = -1;
static int hold_piece = -1;
static int hold_used = 0;
static int speed_ms = 600;
static int game_over = 0;

/* Direct2D objects */
static ID2D1Factory* d2d_factory = NULL;
static ID2D1HwndRenderTarget* render_target = NULL;
static ID2D1SolidColorBrush* brushes[8] = { NULL }; /* 0 = empty, 1..7 pieces */
static ID2D1SolidColorBrush* brush_border = NULL;
static ID2D1SolidColorBrush* brush_bg = NULL;
static ID2D1SolidColorBrush* brush_label = NULL;// 레이블 색 (예: 회색)
static ID2D1SolidColorBrush* brush_label_score = NULL;// 레이블 색 (예: 회색)
static ID2D1SolidColorBrush* brush_label_lines = NULL;// 레이블 색 (예: 회색)
static ID2D1SolidColorBrush* brush_label_level = NULL;// 레이블 색 (예: 회색)
static ID2D1SolidColorBrush* brush_value = NULL;// 점수값 색 (예: 검정)

/* DirectWrite objects (text) */
static IDWriteFactory* dwrite_factory = NULL;
static IDWriteTextFormat* text_format = NULL;

/* Layout */
static const int cell_size = 28;
static const int cell_gap = 2;
static const int board_left = 16;
static const int board_top = 48;
static const int side_panel_left_offset = WIDTH * (cell_size + cell_gap) + 48;

/* forward */
static void update_speed(void);
static int fits_piece(int piece, int px, int py, int rot);
static void lock_piece(void);
static int clear_lines(void);
static void spawn_piece(void);
static void set_current_piece(int piece);
static void game_tick(HWND hwnd);
static void create_d2d_resources(HWND hwnd);
static void discard_d2d_resources(void);
static void on_paint(HWND hwnd);
static void reset_timer(HWND hwnd);

/* Helpers */
static void safe_release(IUnknown* p) {
    if (p) p->Release();
}

/* Rotate a 4x4 mask 90deg clockwise once */
static uint16_t rotate_mask_once(uint16_t m) {
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

/* Rotate mask by rot * 90deg clockwise */
static uint16_t rotate_mask(uint16_t m, int rot) {
    rot &= 3;
    uint16_t r = m;
    for (int i = 0; i < rot; i++) r = rotate_mask_once(r);
    return r;
}

/* Get the mask for piece at rotation rot (compute from canonical mask[0]) */
static uint16_t get_mask(int piece, int rot) {
    if (piece < 0 || piece >= 7) return 0;
    uint16_t base = pieces[piece].mask[0];
    return rotate_mask(base, rot & 3);
}

/* Check whether the given piece (with rotation) fits at px,py on board */
static int fits_piece(int piece, int px, int py, int rot) {
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

/* Clear full lines */
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

/* create brushes/colors for pieces and text resources */
static void create_d2d_resources(HWND hwnd) {
    if (render_target) return;
    if (!d2d_factory) {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
    }
    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    d2d_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &render_target);

    /* brushes: map piece ids to colors */
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Aqua), &brushes[1]);   // I - cyan
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &brushes[2]); // O - yellow
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.6f, 0.2f, 0.8f), &brushes[3]);     // T - purple
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Lime), &brushes[4]);   // S - green
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &brushes[5]);    // Z - red
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), &brushes[6]);   // J - blue
    render_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.5f, 0.0f), &brushes[7]);    // L - orange

    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::GhostWhite), &brush_border);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.85f), &brush_bg);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DarkGreen), &brush_label_score);   // "Score:", "Level:", "Lines:" 같은 레이블
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DeepSkyBlue), &brush_label_level);   // "Score:", "Level:", "Lines:" 같은 레이블
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DarkBlue), &brush_label_lines);   // "Score:", "Level:", "Lines:" 같은 레이블
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::RoyalBlue), &brush_value);  // 숫자 값
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray), &brush_label); // 추가된 부분
    /* Create DirectWrite factory and text format if needed */
    if (!dwrite_factory) {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&dwrite_factory));
    }
    if (dwrite_factory && !text_format) {
        dwrite_factory->CreateTextFormat(
            L"Segoe UI",                // font family
            NULL,                       // font collection
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            18.0f,                      // font size
            L"en-US",                   // locale
            &text_format);
        if (text_format) {
            text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }
    }
}

/* release D2D and DWrite resources */
static void discard_d2d_resources(void) {
    for (int i = 0; i < 8; i++) {
        safe_release((IUnknown*)brushes[i]);
        brushes[i] = NULL;
    }
    safe_release((IUnknown*)brush_border);
    brush_border = NULL;
    safe_release((IUnknown*)brush_bg);
    brush_bg = NULL;
    safe_release((IUnknown*)brush_label);
    brush_label = NULL;
    safe_release((IUnknown*)brush_label_score);
    brush_label_score = NULL;
    safe_release((IUnknown*)brush_label_level);
    brush_label_level = NULL;
    safe_release((IUnknown*)brush_label_lines);
    brush_label_lines = NULL;
    safe_release((IUnknown*)brush_value);
    brush_value = NULL;
    safe_release((IUnknown*)render_target);
    render_target = NULL;

    /* release text resources */
    safe_release((IUnknown*)text_format);
    text_format = NULL;
    safe_release((IUnknown*)dwrite_factory);
    dwrite_factory = NULL;

    safe_release((IUnknown*)d2d_factory);
    d2d_factory = NULL;
}

/* draw helper: fill cell at board coords (bx,by) with brush id */
static void draw_cell(ID2D1RenderTarget* rt, int bx, int by, ID2D1SolidColorBrush* brush) {
    float x = (float)(board_left + bx * (cell_size + cell_gap));
    float y = (float)(board_top + by * (cell_size + cell_gap));
    D2D1_RECT_F r = D2D1::RectF(x, y, x + cell_size, y + cell_size);
    rt->FillRectangle(&r, brush);
    rt->DrawRectangle(&r, brush_border, 1.0f);
}

/* paint everything using Direct2D */
static void on_paint(HWND hwnd) {
    create_d2d_resources(hwnd);
    if (!render_target) return;

    render_target->BeginDraw();
    render_target->Clear(D2D1::ColorF(D2D1::ColorF::White));

    /* draw background panel - use render target size instead of hardcoded 800x600 */
    D2D1_SIZE_F rtSize = render_target->GetSize();
    D2D1_RECT_F bg_rect = D2D1::RectF(0, 0, rtSize.width, rtSize.height);
    render_target->FillRectangle(&bg_rect, brush_bg);

    /* draw board cells */
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int c = board[y][x];
            if (c) {
                draw_cell(render_target, x, y, brushes[c]);
            } else {
                /* empty cell outline */
                float px = (float)(board_left + x * (cell_size + cell_gap));
                float py = (float)(board_top + y * (cell_size + cell_gap));
                D2D1_RECT_F r = D2D1::RectF(px, py, px + cell_size, py + cell_size);
                render_target->DrawRectangle(&r, brush_border, 0.5f);
            }
        }
    }

    /* draw current falling piece */
    uint16_t m = get_mask(cur_piece, cur_rot);
    for (int b = 0; b < 16; b++) {
        if ((m >> b) & 1u) {
            int bx = b % 4;
            int by = b / 4;
            int ax = bx + cur_x;
            int ay = by + cur_y;
            if (ay >= 0 && ay < HEIGHT && ax >= 0 && ax < WIDTH) {
                draw_cell(render_target, ax, ay, brushes[pieces[cur_piece].color]);
            }
        }
    }

    /* draw side panel: Next */
    FLOAT sx = (FLOAT)side_panel_left_offset;
    FLOAT sy = (FLOAT)board_top;
    if (text_format)
        render_target->DrawTextW(L"Next:", 5, text_format, D2D1::RectF(sx, sy - 28, sx + 200, sy), brush_border);
    if (next_piece >= 0) {
        uint16_t nm = get_mask(next_piece, 0);
        for (int b = 0; b < 16; b++) {
            if ((nm >> b) & 1u) {
                int bx = b % 4;
                int by = b / 4;
                /* draw small next preview with same cell size */
                float px = sx + bx * (cell_size / 1.5f + 2.0f);
                float py = sy + by * (cell_size / 1.5f + 2.0f);
                D2D1_RECT_F r = D2D1::RectF(px, py, px + cell_size / 1.5f, py + cell_size / 1.5f);
                render_target->FillRectangle(&r, brushes[pieces[next_piece].color]);
                render_target->DrawRectangle(&r, brush_border);
            }
        }
    }

    /* draw side panel: Hold */
    FLOAT hx = sx;
    FLOAT hy = sy + 160;
    if (text_format)
        render_target->DrawTextW(L"Hold:", 5, text_format, D2D1::RectF(hx, hy - 28, hx + 200, hy), brush_border);
    if (hold_piece >= 0) {
        uint16_t hm = get_mask(hold_piece, 0);
        for (int b = 0; b < 16; b++) {
            if ((hm >> b) & 1u) {
                int bx = b % 4;
                int by = b / 4;
                float px = hx + bx * (cell_size / 1.5f + 2.0f);
                float py = hy + by * (cell_size / 1.5f + 2.0f);
                D2D1_RECT_F r = D2D1::RectF(px, py, px + cell_size / 1.5f, py + cell_size / 1.5f);
                render_target->FillRectangle(&r, brushes[pieces[hold_piece].color]);
                render_target->DrawRectangle(&r, brush_border);
            }
        }
    }

    /* draw HUD text */
    wchar_t hud[128];
    swprintf(hud, 128, L"%d", score);
    if (text_format) {
        render_target->DrawTextW(L"Score:", 6, text_format, D2D1::RectF(sx, hy + 120, sx + 360, hy + 140), brush_label_score);
        render_target->DrawTextW(hud, (UINT32)wcslen(hud), text_format, D2D1::RectF(sx + 80, hy + 120, sx + 300, hy + 160), brush_label_score);
    }
    swprintf(hud, 128, L"%d", level);
    if (text_format) {
        render_target->DrawTextW(L"Level:", 6, text_format, D2D1::RectF(sx, hy + 140, sx + 360, hy + 160), brush_label_level);
        render_target->DrawTextW(hud, (UINT32)wcslen(hud), text_format, D2D1::RectF(sx + 80, hy + 140, sx + 300, hy + 180), brush_label_level);
    }
    swprintf(hud, 128, L"%d", lines_total);
    if (text_format) {
        render_target->DrawTextW(L"Lines:", 6, text_format, D2D1::RectF(sx, hy + 160, sx + 360, hy + 180), brush_label_lines);
        render_target->DrawTextW(hud, (UINT32)wcslen(hud), text_format, D2D1::RectF(sx + 80, hy + 160, sx + 300, hy + 200), brush_label_lines);
    }

    HRESULT hr = render_target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        discard_d2d_resources();
    }
}

/* reset SetTimer with current speed_ms */
static void reset_timer(HWND hwnd) {
    KillTimer(hwnd, 1);
    SetTimer(hwnd, 1, speed_ms, NULL);
}

/* game tick called from WM_TIMER */
static void game_tick(HWND hwnd) {
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

/* window procedure */
static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        create_d2d_resources(hwnd);
        reset_timer(hwnd);
        return 0;
    case WM_SIZE:
        if (render_target) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
            render_target->Resize(size);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_TIMER:
        if (wparam == 1 && !game_over) {
            game_tick(hwnd);
        }
        return 0;
    case WM_KEYDOWN:
        switch (wparam) {
        case VK_LEFT:
            if (fits_piece(cur_piece, cur_x - 1, cur_y, cur_rot)) cur_x--;
            break;
        case VK_RIGHT:
            if (fits_piece(cur_piece, cur_x + 1, cur_y, cur_rot)) cur_x++;
            break;
        case VK_UP: { /* rotate with simple wall-kicks */
            int nr = (cur_rot + 1) % 4;
            /* simple kick table: try staying, then left/right shifts, then small upward shifts */
            const int kicks[8][2] = {
                {0,0}, {-1,0}, {1,0}, {-2,0}, {2,0}, {0,-1}, {-1,-1}, {1,-1}
            };
            for (int ki = 0; ki < 8; ki++) {
                int kx = kicks[ki][0];
                int ky = kicks[ki][1];
                if (fits_piece(cur_piece, cur_x + kx, cur_y + ky, nr)) {
                    cur_x += kx;
                    cur_y += ky;
                    cur_rot = nr;
                    break;
                }
            }
            break;
        }
        case VK_DOWN:
            if (fits_piece(cur_piece, cur_x, cur_y + 1, cur_rot)) {
                cur_y++;
                score += 1;
            }
            break;
        case VK_SPACE: { /* hard drop */
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
                    const int line_scores[5] = {0, 100, 300, 500, 800};
                    score += line_scores[cleared] * level;
                    lines_total += cleared;
                    level = lines_total / 10 + 1;
                    update_speed();
                    reset_timer(hwnd);
                }
            }
            spawn_piece();
            break;
        }
        case 'C':
        case 'c':
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
            }
            break;
        case 'Q':
        case 'q':
            PostQuitMessage(0);
            break;
        default:
            break;
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_PAINT:
        on_paint(hwnd);
        ValidateRect(hwnd, NULL);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        discard_d2d_resources();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

/* entry point */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    srand((unsigned)time(NULL));
    next_piece = -1;
    level = 1;
    lines_total = 0;
    update_speed();
    spawn_piece();

    const wchar_t CLASS_NAME[] = L"TetrisWindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Tetris - Direct2D",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 720,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!hwnd) return 0;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}