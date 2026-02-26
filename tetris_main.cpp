#include "tetris.h"
#include <time.h>
#include <stdlib.h>

void reset_timer(HWND hwnd) {
    KillTimer(hwnd, 1);
    SetTimer(hwnd, 1, speed_ms, NULL);
}

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
        case VK_UP: {
            int nr = (cur_rot + 1) % 4;
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
        case VK_SPACE: {
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