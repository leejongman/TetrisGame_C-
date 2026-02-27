#include "tetris.h"
#include <wchar.h>
#include <cmath>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

void create_d2d_resources(HWND hwnd) {
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
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.8f, 0.9f), &brushes[1]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.85f, 0.2f), &brushes[2]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.3f, 0.9f), &brushes[3]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.9f, 0.5f), &brushes[4]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.3f, 0.3f), &brushes[5]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.6f, 1.0f), &brushes[6]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.6f, 0.2f), &brushes[7]);

    render_target->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.7f, 0.7f), &brush_border);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.12f), &brush_bg);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.85f, 0.6f), &brush_label_score);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.8f, 1.0f), &brush_label_level);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.6f, 1.0f), &brush_label_lines);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.9f, 1.0f), &brush_value);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.85f, 0.85f), &brush_label);

    if (!dwrite_factory) {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&dwrite_factory));
    }
    if (dwrite_factory && !text_format) {
        dwrite_factory->CreateTextFormat(
            L"Segoe UI",
            NULL,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            18.0f,
            L"en-US",
            &text_format);
        if (text_format) {
            text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }
    }
}

void discard_d2d_resources(void) {
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

    safe_release((IUnknown*)text_format);
    text_format = NULL;
    safe_release((IUnknown*)dwrite_factory);
    dwrite_factory = NULL;

    safe_release((IUnknown*)d2d_factory);
    d2d_factory = NULL;
}

void draw_cell(ID2D1RenderTarget* rt, int bx, int by, ID2D1SolidColorBrush* brush) {
    float x = (float)(board_left + bx * (cell_size + cell_gap));
    float y = (float)(board_top + by * (cell_size + cell_gap));
    D2D1_RECT_F r = D2D1::RectF(x, y, x + cell_size, y + cell_size);

    /* Main fill */
    rt->FillRectangle(&r, brush);

    /* Inner highlight - bright edges for 3D effect */
    ID2D1SolidColorBrush* highlight = NULL;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.3f), &highlight);

    /* Top highlight - smaller size to fit cell */
    D2D1_RECT_F top_highlight = D2D1::RectF(x + 1, y + 1, x + cell_size - 1, y + 5);
    rt->FillRectangle(&top_highlight, highlight);

    /* Left highlight */
    D2D1_RECT_F left_highlight = D2D1::RectF(x + 1, y + 1, x + 5, y + cell_size - 1);
    rt->FillRectangle(&left_highlight, highlight);

    if (highlight) highlight->Release();

    /* Dark shadow - bottom and right edges */
    ID2D1SolidColorBrush* shadow = NULL;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.4f), &shadow);

    /* Bottom shadow */
    D2D1_RECT_F bottom_shadow = D2D1::RectF(x + 1, y + cell_size - 5, x + cell_size - 1, y + cell_size - 1);
    rt->FillRectangle(&bottom_shadow, shadow);

    /* Right shadow */
    D2D1_RECT_F right_shadow = D2D1::RectF(x + cell_size - 5, y + 1, x + cell_size - 1, y + cell_size - 1);
    rt->FillRectangle(&right_shadow, shadow);

    if (shadow) shadow->Release();

    /* Border outline */
    ID2D1SolidColorBrush* border = NULL;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.6f), &border);
    rt->DrawRectangle(&r, border, 0.8f);
    if (border) border->Release();
}

void on_paint(HWND hwnd) {
    create_d2d_resources(hwnd);
    if (!render_target) return;

    render_target->BeginDraw();
    render_target->Clear(D2D1::ColorF(0.08f, 0.08f, 0.08f));

    /* Draw modern gradient background for game board */
    int board_width = WIDTH * (cell_size + cell_gap) + cell_gap;
    int board_height = HEIGHT * (cell_size + cell_gap) + cell_gap;
    D2D1_RECT_F board_bg = D2D1::RectF(
        (float)board_left - 4,
        (float)board_top - 4,
        (float)(board_left + board_width + 4),
        (float)(board_top + board_height + 4)
    );

    /* Modern gradient effect - strong depth for 3D effect */
    float pulse = 0.04f * sinf((animation_frame % 60) * 3.14159f / 30.0f);

    /* Create enhanced gradient effect with better depth */
    float step_height = board_bg.bottom - board_bg.top;
    for (float i = 0; i < step_height; i += 1.5f) {
        float progress = i / step_height;
        /* Darker at top, lighter at bottom for depth */
        float r = 0.10f + (0.22f - 0.10f) * progress + pulse * 0.5f;
        float g = 0.12f + (0.24f - 0.12f) * progress + pulse * 0.5f;
        float b = 0.15f + (0.28f - 0.15f) * progress + pulse * 0.5f;

        ID2D1SolidColorBrush* line_brush = NULL;
        render_target->CreateSolidColorBrush(D2D1::ColorF(r, g, b), &line_brush);
        D2D1_RECT_F line_rect = D2D1::RectF(
            board_bg.left,
            board_bg.top + i,
            board_bg.right,
            board_bg.top + i + 1.5f
        );
        render_target->FillRectangle(&line_rect, line_brush);
        if (line_brush) line_brush->Release();
    }

    /* Add inner shadow for depth */
    ID2D1SolidColorBrush* inner_shadow = NULL;
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.2f), &inner_shadow);
    D2D1_RECT_F inner_shadow_rect = D2D1::RectF(
        board_bg.left,
        board_bg.top,
        board_bg.right,
        board_bg.top + 8.0f
    );
    render_target->FillRectangle(&inner_shadow_rect, inner_shadow);
    if (inner_shadow) inner_shadow->Release();

    /* Add highlight at top for beveled edge */
    ID2D1SolidColorBrush* highlight_brush = NULL;
    render_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f), &highlight_brush);
    D2D1_POINT_2F p1 = D2D1::Point2F(board_bg.left, board_bg.top + 1);
    D2D1_POINT_2F p2 = D2D1::Point2F(board_bg.right, board_bg.top + 1);
    render_target->DrawLine(p1, p2, highlight_brush, 1.5f);
    if (highlight_brush) highlight_brush->Release();

    /* Add dark border at bottom for depth */
    ID2D1SolidColorBrush* border_brush = NULL;
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.4f), &border_brush);
    p1 = D2D1::Point2F(board_bg.left, board_bg.bottom - 1);
    p2 = D2D1::Point2F(board_bg.right, board_bg.bottom - 1);
    render_target->DrawLine(p1, p2, border_brush, 1.5f);
    if (border_brush) border_brush->Release();

    /* draw board cells */
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int c = board[y][x];
            if (c) {
                draw_cell(render_target, x, y, brushes[c]);
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

    /* Draw animated background for side panel */
    float hue_shift = (animation_frame % 120) / 120.0f;
    float panel_brightness = 0.3f + 0.1f * sinf(hue_shift * 3.14159f * 2.0f);
    ID2D1SolidColorBrush* panel_bg_brush = NULL;
    render_target->CreateSolidColorBrush(
        D2D1::ColorF(0.13f + panel_brightness * 0.05f, 0.13f + panel_brightness * 0.05f, 0.16f + panel_brightness * 0.05f),
        &panel_bg_brush
    );

    /* Panel background covering all side info */
    D2D1_RECT_F panel_rect = D2D1::RectF(sx - 10, sy - 40, sx + 150, sy + 260);
    render_target->FillRectangle(&panel_rect, panel_bg_brush);

    /* Panel top highlight for 3D bevel */
    ID2D1SolidColorBrush* panel_highlight = NULL;
    render_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), &panel_highlight);
    D2D1_RECT_F panel_top_highlight = D2D1::RectF(sx - 10, sy - 40, sx + 150, sy - 35);
    render_target->FillRectangle(&panel_top_highlight, panel_highlight);
    if (panel_highlight) panel_highlight->Release();

    /* Panel left highlight */
    ID2D1SolidColorBrush* panel_left_highlight = NULL;
    render_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f), &panel_left_highlight);
    D2D1_RECT_F panel_left_highlight_rect = D2D1::RectF(sx - 10, sy - 40, sx - 6, sy + 260);
    render_target->FillRectangle(&panel_left_highlight_rect, panel_left_highlight);
    if (panel_left_highlight) panel_left_highlight->Release();

    /* Panel bottom shadow */
    ID2D1SolidColorBrush* panel_shadow = NULL;
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.3f), &panel_shadow);
    D2D1_RECT_F panel_bottom_shadow = D2D1::RectF(sx - 10, sy + 255, sx + 150, sy + 260);
    render_target->FillRectangle(&panel_bottom_shadow, panel_shadow);
    if (panel_shadow) panel_shadow->Release();

    /* Panel right shadow */
    ID2D1SolidColorBrush* panel_right_shadow = NULL;
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.25f), &panel_right_shadow);
    D2D1_RECT_F panel_right_shadow_rect = D2D1::RectF(sx + 146, sy - 40, sx + 150, sy + 260);
    render_target->FillRectangle(&panel_right_shadow_rect, panel_right_shadow);
    if (panel_right_shadow) panel_right_shadow->Release();

    /* Draw border with gradient effect */
    ID2D1SolidColorBrush* panel_border = NULL;
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.35f, 0.35f, 0.35f), &panel_border);
    render_target->DrawRectangle(&panel_rect, panel_border, 1.5f);
    if (panel_border) panel_border->Release();

    if (panel_bg_brush) panel_bg_brush->Release();

    if (text_format)
        render_target->DrawTextW(L"Next:", 5, text_format, D2D1::RectF(sx, sy - 28, sx + 200, sy), brush_border);
    if (next_piece >= 0) {
        uint16_t nm = get_mask(next_piece, 0);
        for (int b = 0; b < 16; b++) {
            if ((nm >> b) & 1u) {
                int bx = b % 4;
                int by = b / 4;
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