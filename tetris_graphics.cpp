#include "tetris.h"
#include <wchar.h>

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
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Aqua), &brushes[1]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &brushes[2]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.6f, 0.2f, 0.8f), &brushes[3]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Lime), &brushes[4]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &brushes[5]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), &brushes[6]);
    render_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.5f, 0.0f), &brushes[7]);

    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::GhostWhite), &brush_border);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.85f), &brush_bg);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DarkGreen), &brush_label_score);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DeepSkyBlue), &brush_label_level);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DarkBlue), &brush_label_lines);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::RoyalBlue), &brush_value);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray), &brush_label);

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
    rt->FillRectangle(&r, brush);
    rt->DrawRectangle(&r, brush_border, 1.0f);
}

void on_paint(HWND hwnd) {
    create_d2d_resources(hwnd);
    if (!render_target) return;

    render_target->BeginDraw();
    render_target->Clear(D2D1::ColorF(D2D1::ColorF::White));

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